/**
 * @file cmd.h
 * @author Timothy Nguyen
 * @brief Parse and execute commands received from console.
 * @version 0.1
 * @date 2021-07-16
 * 
 * Clients that wish to have commands invoked from user should register their callback functions here.
 */

#ifndef _CMD_H_
#define _CMD_H_

#include "common.h"
#include "cmd.h"
#include "active.h"

/* Configuration parameters */
#define CMD_MAX_CLIENTS 10 // Maximum number of clients/modules using the command module.
#define CMD_MAX_TOKENS 10  // Maximum number of command tokens.

#define CMD_THREAD_SIZE 1024  // Command active object thread size.
#define CMD_EVENT_MSG_COUNT 5 // Maximum number of messages in event message queue.

/* Function signature for a command handler function. */
typedef uint32_t (*cmd_cb_t)(uint32_t argc, const char **argv);

/* Information about a single command, provided by the client */
typedef struct
{
    const char *const cmd_name; // Name of command
    const cmd_cb_t cb;          // Command callback function
    const char *const help;     // Command help string
} cmd_cmd_info;

/* Information about the client. */
typedef struct
{
    const char *const client_name;         // Client name (first command line token)
    const uint32_t num_cmds;               // Number of commands.
    const cmd_cmd_info *const cmds;        // Command information.
    const uint32_t num_u16_pms;            // Number of performance measurement values.
    uint16_t *const u16_pms;               // Performance measurement values.
    const char *const *const u16_pm_names; // Performance measurement names.
} cmd_client_info;

/* Command event signals */
enum cmd_signals
{
    CMD_RX_SIG = USER_SIG, // Command received from user over serial.
};

/* Derived command event class */
typedef struct
{
    Event base; // Inherit base event class.

    /* Private attributes */
    char *cmd_line; // Pointer to command string.
} Cmd_Event;

/* Argument representations */
typedef struct
{
    char type;
    union
    {
        void *p;
        uint8_t *p8;
        uint16_t *p16;
        uint32_t *p32;
        int32_t i;
        uint32_t u;
        const char *s;
    } val;
} cmd_arg_val;

/**
 * @brief Base active class attribute of command active object.
 *
 * Other modules may post events to command active object using Active_post.
 */
extern Active *cmd_base;

/** 
 * @brief Initialize cmd instance.
 *
 * @return MOD_OK for success, else a "MOD_ERR" value. 
 */
mod_err_t cmd_init(void);

/**
 * @brief Create command active object thread.
 *
 * @return MOD_OK if successful, else a "MOD_ERR" value
 *
 * @note Function does not start scheduler.
 */
mod_err_t cmd_start(void);

/** 
 * @brief Register a client.
 *
 * @param _client_info The client's configuration (eg. command metadata).
 *
 * @return MOD_OK for success, else a "MOD_ERR" value.
 * 
 * @note cmd_client_info should be declared static since module stores a copy of the pointer.
 */
mod_err_t cmd_register(const cmd_client_info *_client_info);

/**
 * @brief Parse and validate command arguments
 *
 * @param[in]  argc The number of arguments to be parsed.
 * @param[in]  argv Pointer string array of arguments to be be parsed.
 * @param[in]  fmt String indicating expected argument types
 * @param[out] arg_vals Pointer to array of parsed argument values
 *
 * @return Number of parsed arguments, negative value if error.
 *
 * @note In case of error, an error message is always printed to the
 * console.
 *
 * This function provides a common method to parse and validate command
 * arguments.  In particular, it does string to numeric conversions, with error
 * checking.
 *
 * A format string is used to guide the parsing. The format string contains a
 * letter for each expected argument. The supported letters are:
 * - i Integer value, in either decimal, octal, or hex formats. Octal values
 *     must start with 0, and hex values must start with 0x.
 * - u Unsigned value, in either decimal, octal, or hex formats. Octal values
 *     must start with 0, and hex values must start with 0x.
 * - p Pointer, in hex format. No leading 0x is necessary (but allowed).
 * - s String
 *
 * In addition:
 * - [ indicates that remaining arguments are optional. However,
 *   if one optional argument is present, then subsequent arguments
 *   are required.
 * - ] is ignored; it can be put into the fmt string for readability,
 *   to match brackets.
 *
 * Examples:
 *   "up" - Requires exactly one unsigned and one pointer argument.
 *   "ii" - Requires exactly two integer arguments.
 *   "i[i" - Requires either one or two integer arguments.
 *   "i[i]" - Same as above (matched brackets).
 *   "i[i[i" - Requires either one, two, or three integer arguments.
 *   "i[i[i]]" - Same as above (matched brackets).
 *   "i[ii" - Requires either one or three integer arguments.
 *   "i[ii]" - Same as above (matched brackets).
 *
 * @return On success, the number of arguments present (>=0), a "MOD_ERR" value
 *         (<0). See code for details.
 */
int32_t cmd_parse_args(int32_t argc, const char **argv, const char *fmt, cmd_arg_val *arg_vals);

#endif
