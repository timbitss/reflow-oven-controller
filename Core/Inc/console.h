/**
 * @file console.h
 * @author Command line interface.
 * @brief Timothy Nguyen
 * @version 0.1
 * @date 2021-07-15
 * 
 * Console module processes characters over serial and executes
 * appropriate command once Enter key is pressed.
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "common.h"
#include "cmsis_os.h"

/* Configuration parameters */
#define CONSOLE_MSG_QUEUE_SIZE 1024    // Size of message queue for UART serial characters to be processed.
#define CONSOLE_CMD_BUF_SIZE 40        // Size of buffer to hold processed command line characters.
#define CONSOLE_THREAD_STACK_SIZE 1024 // Stack size for console thread.

#define PROMPT "> "

/**
 * @brief Initialize console module instance.
 *
 * @return MOD_OK for success.
 */
mod_err_t console_init(void);

/**
 * @brief Create command thread and message queue.
 *
 * @return MOD_OK for success, a "MOD_ERR" value otherwise.
 *
 * @note Function does not start scheduler.
 */
mod_err_t console_start(void);

/**
 * @brief Post character to console's message queue (non-blocking).
 *
 * @param c Character to transmit.
 *
 * @return MOD_OK if successful, MOD_ERR_TIMEOUT if message queue is full.
 */
mod_err_t console_post(char c);

#endif
