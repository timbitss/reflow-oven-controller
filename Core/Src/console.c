/**
 * @file console.c
 * @author Command line interface.
 * @brief Timothy Nguyen
 * @version 0.1
 * @date 2021-07-15
 */

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "console.h"
#include "common.h"
#include "uart.h"
#include "cmd.h"
#include "log.h"
#include "printf.h"
#include "active.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

/* Console active object structure */
typedef struct
{
    /* OS objects */
    osThreadId_t console_thread_id;      // Console thread ID.
    osMessageQueueId_t console_queue_id; // Console message queue ID.
    osSemaphoreId_t console_sem_id;      // Console semaphore ID.

    /* Private attributes */
    char rx_char;                       // Received character from message queue.
    char cmd_buf[CONSOLE_CMD_BUF_SIZE]; // Hold command characters as they are entered by user over serial.
    uint16_t num_cmd_buf_chars;         // Holds number of characters currently in command buffer.
    bool first_run_done;                // First run, print PROMPT before checking for command characters
} Console_t;

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static inline void post_cmd_event(void); // Post command event to command active object.

static void Console_thread(void *argument); // Console thread function.

static inline mod_err_t console_process(char c); // Process received character from UART queue.

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

/* Console_t object */
static Console_t console;

/* Unique tag for logging module */
static const char *TAG = "CONSOLE";

/* Command event object to send when Enter key is pressed. */
static Cmd_Event cmd_evt = {.base = {CMD_RX_SIG}, .cmd_line = console.cmd_buf};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Initialize console module instance.
 *
 * @return MOD_OK for success.
 */
mod_err_t console_init(void)
{
    memset(&console, 0, sizeof(console));
    LOGI(TAG, "Initialized console.");
    return MOD_OK;
}

mod_err_t console_start(void)
{
    /* Create OS objects */
    static const osThreadAttr_t thread_attr = {.stack_size = CONSOLE_THREAD_STACK_SIZE};
    console.console_thread_id = osThreadNew(Console_thread, NULL, &thread_attr);
    console.console_queue_id = osMessageQueueNew(CONSOLE_MSG_QUEUE_SIZE, sizeof(char), NULL);

    ASSERT(console.console_queue_id != NULL && console.console_thread_id != NULL);

    uart_start();

    return MOD_OK;
}

mod_err_t console_post(char c)
{
    osStatus_t err = osMessageQueuePut(console.console_queue_id, &c, 0U, 0U);
    if (err != osOK)
    {
        return MOD_ERR_TIMEOUT;
    }

    return MOD_OK;
}

void console_signal(void)
{
    osSemaphoreRelease(console.console_sem_id);
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Console thread.
 */
static void Console_thread(void *argument)
{
    LOG(PROMPT);
    while (1)
    {
        /* Read character from message queue, then process character */
        char char_to_process = 0;
        osStatus_t status = osMessageQueueGet(console.console_queue_id, &char_to_process, NULL, osWaitForever);

        if (status != osOK)
        {
            LOGE(TAG, "Could not read character from queue.");
        }
        else
        {
            console_process(char_to_process);
        }
    }
}

/**
 * @brief Process received character from UART peripheral.
 *
 * @param c Character to process.
 *
 * @return MOD_OK if successful, otherwise a "MOD_ERR" value.
 */
static mod_err_t console_process(char c)
{

    /* Execute command once Enter key is pressed. */
    if (c == '\n' || c == '\r')
    {
        console.cmd_buf[console.num_cmd_buf_chars] = '\0'; // Signal end of command string.
        LOG("\r\n");
        post_cmd_event();
        console.num_cmd_buf_chars = 0;
        return MOD_OK;
    }
    /* Delete a character when Backspace key is pressed. */
    if (c == '\b' || c == '\x7f')
    {
        if (console.num_cmd_buf_chars > 0)
        {
            LOG("\x7f");
            console.num_cmd_buf_chars--; // "Overwrite" last character.
        }
        return MOD_OK;
    }
    /* Toggle logging on and off LOG_TOGGLE_CHAR key is pressed. */
    if (c == LOG_TOGGLE_CHAR)
    {
        bool log_active = log_toggle();
        LOG("<Logging %s>\r\n", log_active ? "on" : "off");
        return MOD_OK;
    }
    /* Echo the character back. */
    if (isprint(c))
    {
        if (console.num_cmd_buf_chars < (CONSOLE_CMD_BUF_SIZE - 1))
        {
            console.cmd_buf[console.num_cmd_buf_chars++] = c;
            LOG("%c", c);
        }
        else
        {
            /* No space in buffer, so ring terminal bell. */
            LOGW(TAG, "No more space in command buffer.");
            LOG("\a");
        }
        return MOD_OK;
    }

    return MOD_OK;
}

/**
 * @brief Post command event to command active object.
 */
static inline void post_cmd_event(void)
{
    Active_post(cmd_base, (Event const *)&cmd_evt);
}
