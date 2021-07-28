/**
 * @file cmd.h
 * @author Timothy Nguyen
 * @brief Parse and execute commands received from console.
 * @version 0.1
 * @date 2021-07-16
 */

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cmd.h"
#include "log.h"
#include "printf.h"
#include "active.h"
#include "console.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

/* Command active object class */
typedef struct
{
    Active base; // Inherited base active object class

    /* Private attributes */
    char cmd_buf[CONSOLE_CMD_BUF_SIZE]; // Command line buffer.
} Cmd_Active;

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static mod_err_t tokenize(char *str_to_tokenize, const char **tokens, uint32_t *num_tokens); // Tokenize string.
static mod_err_t help_handler(const char **tokens);                                          // Handle global help command.
static mod_err_t client_command_handler();                                                   // Handle client command.

/* Active object event handler */
static void Cmd_Event_Handler(Cmd_Active *const ao, Cmd_Event const *const evt);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

/* Hold information about each client */
static const cmd_client_info *client_infos[CMD_MAX_CLIENTS];

/* Unique tag for logging module */
static const char *TAG = "CMD";

/* Command active object */
static Cmd_Active cmd_ao;

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Base active class attribute of command active object.
 *
 * Other modules may post events to command active object using Active_post.
 */
Active *cmd_base;

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

mod_err_t cmd_init(void)
{
    Active_ctor((Active *)&cmd_ao, (EventHandler)&Cmd_Event_Handler); // Call base active object constructor.
    cmd_base = &(cmd_ao.base);
    memset(cmd_ao.cmd_buf, 0, CONSOLE_CMD_BUF_SIZE); // Initialize private variables.
    LOGI(TAG, "Initialized command.");
    return MOD_OK;
}

mod_err_t cmd_start()
{
    static const osThreadAttr_t thread_attr = {.stack_size = CMD_THREAD_SIZE};
    return Active_start((Active *)&cmd_ao, &thread_attr, CMD_EVENT_MSG_COUNT, NULL);
}

mod_err_t cmd_register(const cmd_client_info *_client_info)
{
    for (uint8_t i = 0; i < CMD_MAX_CLIENTS; i++)
    {
        if (client_infos[i] == NULL || strcasecmp(client_infos[i]->client_name, _client_info->client_name) == 0)
        {
            client_infos[i] = _client_info;
            LOGI(TAG, "Registered commands for %s module", client_infos[i]->client_name);
            return MOD_OK;
        }
    }
    return MOD_ERR_RESOURCE;
}

int32_t cmd_parse_args(int32_t argc, const char **argv, const char *fmt, cmd_arg_val *arg_vals)
{
    int32_t arg_cnt = 0;
    char *endptr;
    bool opt_args = false;

    while (*fmt)
    {
        if (*fmt == '[')
        {
            opt_args = true;
            fmt++;
            continue;
        }
        if (*fmt == ']')
        {
            fmt++;
            continue;
        }

        if (arg_cnt >= argc)
        {
            if (opt_args)
            {
                return arg_cnt;
            }
            printf("Insufficient arguments\r\n");
            return MOD_ERR_BAD_CMD;
        }

        // These error conditions should not occur, but we check them for
        // safety.
        if (*argv == NULL || **argv == '\0')
        {
            printf("Invalid empty arguments\r\n");
            return MOD_ERR_BAD_CMD;
        }

        switch (*fmt)
        {
        case 'i':
            arg_vals->val.i = strtol(*argv, &endptr, 0);
            if (*endptr)
            {
                printf("Argument '%s' not a valid integer\r\n", *argv);
                return MOD_ERR_ARG;
            }
            break;
        case 'u':
            arg_vals->val.u = strtoul(*argv, &endptr, 0);
            if (*endptr)
            {
                printf("Argument '%s' not a valid unsigned integer\r\n", *argv);
                return MOD_ERR_ARG;
            }
            break;
        case 'p':
            arg_vals->val.p = (void *)strtoul(*argv, &endptr, 16);
            if (*endptr)
            {
                printf("Argument '%s' not a valid pointer\r\n", *argv);
                return MOD_ERR_ARG;
            }
            break;
        case 's':
            arg_vals->val.s = *argv;
            break;
        default:
            printf("Bad argument format '%c'\n", *fmt);
            return MOD_ERR_ARG;
        }
        arg_vals->type = *fmt;
        arg_vals++;
        arg_cnt++;
        argv++;
        fmt++;
        opt_args = false;
    }
    if (arg_cnt < argc)
    {
        printf("Too many arguments \r\n");
        return MOD_ERR_BAD_CMD;
    }
    return arg_cnt;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Execute a command line.
 *
 * @param cmd_line The command line string.
 *
 * @return MOD_OK for success, else a "MOD_ERR" value.
 *
 * This function parses the command line and then executes the command,
 * typically by running a command function handler for a client.
 */
static mod_err_t cmd_execute(char *cmd_line)
{
    LOGI(TAG, "Command received: %s", cmd_line);
    uint32_t num_tokens = 0;
    const char *tokens[CMD_MAX_TOKENS] = {0}; // Store individual tokens as strings.

    /* Tokenize command line */
    mod_err_t err = tokenize(cmd_line, tokens, &num_tokens);
    if (err)
    {
        return err;
    }

    /* If there are no tokens, nothing to do. */
    if (num_tokens == 0)
    {
        return MOD_OK;
    }

    /* Handle help/? command. */
    err = help_handler(tokens);
    if (err != MOD_DID_NOTHING)
    {
        return err;
    }

    err = client_command_handler(tokens, num_tokens);
    return err;
}

/**
 * @brief Tokenize string.
 *  
 * @param[in] str_to_tokenize String to tokenize.
 * @param[out] tokens  Array to store individual tokens.
 * @param[out] num_tokens Number of tokens gathered.
 * 
 * @return MOD_OK if successful, "MOD_ERR_" otherwise.
 * 
 * Tokens include name of client, command, and arguments passed.
 */
static inline mod_err_t tokenize(char *str_to_tokenize, const char **tokens, uint32_t *num_tokens)
{
    char *ptr = str_to_tokenize;
    uint32_t token_count = 0;

    /* Iterate through each token. */
    while (1)
    {
        /* Find start of token. */
        while (*ptr != '\0' && isspace((unsigned char)*ptr))
        {
            ptr++;
        }

        if (*ptr == '\0') // Found end of line.
        {
            break;
        }
        else
        {
            if (token_count >= CMD_MAX_TOKENS)
            {
                LOGW(TAG, "Too many tokens");
                return MOD_ERR_BAD_CMD;
            }
            else
            {
                /* Record pointer to start of token, then find end of token. */
                tokens[token_count] = ptr;
                ptr++;
                token_count++;
                while (*ptr != '\0' && !isspace((unsigned char)*ptr))
                {
                    ptr++;
                }
                if (*ptr != '\0')
                {
                    *ptr = '\0'; // Terminate end of token.
                    ptr++;
                }
                else
                {
                    break; // Found end of line ('\0'), no more tokens.
                }
            }
        }
    }

    *num_tokens = token_count;
    return MOD_OK;
}

/**
 * @brief Handle global help command.
 * 
 * @param tokens Array of token strings.
 * 
 * @return MOD_OK if successful, 
 *         MOD_DID_NOTHING if not a global help command, 
 *         otherwise a "MOD_ERR" value.
 * 
 * Iterates through each client infos 
 */
static inline mod_err_t help_handler(const char **tokens)
{
    if (strcasecmp("help", tokens[0]) == 0 || strcasecmp("?", tokens[0]) == 0)
    {
        /* Iterate through commands of each client. */
        for (uint8_t i = 0; client_infos[i] != NULL && i < CMD_MAX_CLIENTS; i++)
        {
            const cmd_client_info *ci = client_infos[i];

            LOG("%s (", ci->client_name);

            if (ci->num_u16_pms > 0 && ci->num_cmds == 0)
            {
                /* If client provided pm info only, display pm command. */
                LOG("pm)\r\n");
                continue;
            }
            else if (ci->num_cmds == 0)
            {
                continue;
            }
            else
            {
                uint8_t i2 = 0;
                for (i2 = 0; i2 < ci->num_cmds; i2++)
                {
                    const cmd_cmd_info *cci = &(ci->cmds[i2]);
                    LOG("%s%s", i2 == 0 ? "" : ", ", cci->cmd_name);
                }
                if (ci->num_u16_pms > 0)
                {
                    LOG(", pm");
                }
                LOG(")\r\n");
            }
        }

        return MOD_OK;
    }

    return MOD_DID_NOTHING; // Not a top-level help command.
}

/**
 * @brief Handle client-specific commands.
 * 
 * @param tokens Array of token strings.
 * @param num_tokens Number of tokens.
 *
 * @return MOD_OK if successful, 
 *         MOD_DID_NOTHING if not a client command, 
 *         otherwise a "MOD_ERR" value.
 */
static inline mod_err_t client_command_handler(const char **tokens, uint32_t num_tokens)
{
    for (uint8_t i = 0; client_infos[i] != NULL && i < CMD_MAX_CLIENTS; i++)
    {
        const cmd_client_info *ci = client_infos[i];

        /* Look for correct client first */
        if (strcasecmp(tokens[0], ci->client_name) != 0)
        {
            continue;
        }

        /* If there is no command with client, assume they want help. */
        if (num_tokens == 1)
        {
            tokens[1] = "help";
        }

        /* Handle help command directly. */
        if (strcasecmp(tokens[1], "help") == 0 || strcasecmp(tokens[1], "?") == 0)
        {
            /* Print out all commands associated with client */
            for (uint8_t i2 = 0; i2 < ci->num_cmds; i2++)
            {
                const cmd_cmd_info *cci = &(ci->cmds[i2]);
                LOG("%s %s: %s\r\n", ci->client_name, cci->cmd_name, cci->help);
            }
            /* If client provided pm info, print help for pm command also. */
            if (ci->num_u16_pms > 0)
            {
                LOG("%s pm: get or clear performance measurements, "
                    "args: [clear] \r\n",
                    ci->client_name);
            }

            return MOD_OK;
        }

        /* Handle pm command directly. */
        if (strcasecmp(tokens[1], "pm") == 0)
        {
            if (ci->num_u16_pms > 0)
            {
                bool clear = (num_tokens >= 3 && strcasecmp(tokens[2], "clear") == 0);
                if (clear)
                {
                    LOG("Clearing %s performance measurements\r\n", ci->client_name);
                }
                else
                {
                    LOG("%s pms:\r\n", ci->client_name);
                }
                for (uint8_t i2 = 0; i2 < ci->num_u16_pms; i2++)
                {
                    if (clear)
                    {
                        ci->u16_pms[i2] = 0;
                    }
                    else
                    {
                        LOG("%s: %d\r\n", ci->u16_pm_names[i2], ci->u16_pms[i2]);
                    }
                }
            }

            return MOD_OK;
        }

        /* Look for command within client. */
        for (uint8_t i2 = 0; i2 < ci->num_cmds; i2++)
        {
            if (strcasecmp(tokens[1], ci->cmds[i2].cmd_name) == 0)
            {
                if (num_tokens == 3 && (strcasecmp(tokens[2], "help") == 0 || strcasecmp(tokens[2], "?") == 0))
                {
                    LOG("%s %s: %s\r\n", ci->client_name, ci->cmds[i2].cmd_name, ci->cmds[i2].help);
                }
                else
                {
                    ci->cmds[i2].cb(num_tokens - 2, tokens + 2); // Ignore client and command tokens.
                }
                return MOD_OK;
            }
        }

        LOG("No such command (%s %s)\r\n", tokens[0], tokens[1]);
        return MOD_ERR_BAD_CMD;
    }

    /* Could not find client */
    LOG("No such command: ");
    for (uint8_t i = 0; i < num_tokens; i++)
    {
        LOG("%s ", tokens[i]);
    }
    LOG("\r\n");
    return MOD_ERR_BAD_CMD;
}

/**
 * Command event handler.
 *
 * @param ao Command active object.
 * @param evt Command event object.
 */
static void Cmd_Event_Handler(Cmd_Active *const ao, Cmd_Event const *const evt)
{
    switch (evt->base.sig)
    {
    case INIT_SIG:
        LOGI(TAG, "Command active object initialized.");
        break;
    case CMD_RX_SIG:
        /* Copy command line to avoid race conditions. */
        strncpy(cmd_ao.cmd_buf, evt->cmd_line, CONSOLE_CMD_BUF_SIZE);
        cmd_execute(cmd_ao.cmd_buf);
        break;
    default:
        LOGW(TAG, "Unknown event signal");
        break;
    }
}
