/**
 * @file log.h
 * @author Timothy Nguyen
 * @brief Console logging module.
 * @version 0.1
 * @date 2021-07-16
 * 
 * Currently supports logging messages over UART with timestamps and configurable global log level.
 * 
 * In each module that uses logging functionality, define a TAG variable like so: 
 *
 * static const char* TAG = "MyModule";
 *
 * Then use one of logging macros to produce output, e.g: 
 * 
 * LOGW(TAG, "Baud rate error %.1f%%. Requested: %d baud, actual: %d baud", error * 100, baud_req, baud_real);
 */

#ifndef _LOG_H_
#define _LOG_H_

#include <stdbool.h>

#include "stm32l4xx_hal.h"
#include "common.h"
#include "printf.h"

/* Configuration parameters */
#define LOG_TOGGLE_CHAR '\t' // Press LOG_TOGGLE_CHAR to toggle logging on and off.

/**
 * @brief Logging levels.
 */
typedef enum
{
    LOG_NONE,    // No log output.
    LOG_ERROR,   // Critical errors. Software module can not recover on its own.
    LOG_WARNING, // Errors in which recovery measures have been taken by module.
    LOG_INFO,    // Information messages which describe normal flow of events.
    LOG_DEBUG,   // Extra information which is not necessary for normal use (values, pointers, sizes, etc).
    LOG_VERBOSE, // Bigger chunks of debugging information, or frequent messages which can potentially flood the output.

    LOG_DEFAULT = LOG_INFO // Default log level.
} log_level_t;

/* Logging text colours */
#define LOG_COLOUR_BLACK "30"
#define LOG_COLOUR_RED "31"
#define LOG_COLOUR_GREEN "32"
#define LOG_COLOUR_BROWN "33"
#define LOG_COLOUR_BLUE "34"
#define LOG_COLOUR_PURPLE "35"
#define LOG_COLOUR_CYAN "36"
#define LOG_COLOUR(COLOUR) "\033[0;" COLOUR "m"
#define LOG_BOLD(COLOUR) "\033[1;" COLOUR "m"
#define LOG_RESET_COLOUR "\033[0m\033[K"

#define LOG_COLOUR_E LOG_COLOUR(LOG_COLOUR_RED)
#define LOG_COLOUR_W LOG_COLOUR(LOG_COLOUR_BROWN)
#define LOG_COLOUR_I LOG_COLOUR(LOG_COLOUR_GREEN)
#define LOG_COLOUR_D LOG_COLOUR(LOG_COLOUR_BLUE)
#define LOG_COLOUR_V LOG_COLOUR(LOG_COLOUR_CYAN)

/**
 * @brief Runtime macro to format string with additional information.
 * 
 * @param level First letter of log level.
 * @param format Format string before additional information is prepended.
 * 
 * Additional information prepended from left-to-right: Colour, Log level, Time (ms), Tag. 
 */
#define LOG_FORMAT(letter, format) "\r" LOG_COLOUR_##letter #letter " (%lu.%03lu) %s: " format "\r\n"

#define ASSERTION_FORMAT LOG_COLOUR_E "E (%lu.%03lu) Assertion failed at %s, line %d" \
                                      "\r\n"

/**
 * @brief Initialize log module.
 *
 * @return MOD_OK if successful, otherwise a "MOD_ERR" value.
 */
mod_err_t log_init(void);

/**
 * @brief Toggle data logging.
 * 
 * @return true Logging is now active.
 *         false Logging is now inactive.
 */
bool log_toggle(void);

/**
 * @brief Check if logging is active or inactive.
 * 
 * @return true Logging is active.
 *         false Logging is inactive.
 */
bool log_is_active(void);

/** 
 * @brief Base "printf" style function for logging.
 *
 * @param tag Unique module tag.
 * @param level Message's log level.
 * @param fmt Format string.
 * @param ... Variable arguments.
 *
 * This function is not intended to be used directly. Instead, use one of 
 * LOGE, LOGW, LOGI, LOGD, LOGV macros below.
 */
void log_printf(const char *tag, log_level_t level, const char *fmt, ...);

/* Private variables for logging macros. Do not modify. */
extern bool _log_active;          // Is data logging active or inactive?
extern int32_t _global_log_level; // Only print messages at or below the global log level.

/**
 * @brief Runtime macros to output a log message at a specified level.
 * 
 * @param tag Unique tag associated with module.
 * @param fmt Format string.
 * @param ... Variable arguments. 
 * 
 * @note tag should have static storage duration.
 */
#define LOGE(tag, fmt, ...)                                                                           \
    do                                                                                                \
    {                                                                                                 \
        if (_log_active)                                                                              \
        {                                                                                             \
            uint32_t ms = HAL_GetTick();                                                              \
            log_printf(tag, LOG_ERROR, LOG_FORMAT(E, fmt), ms / 1000, ms % 1000, tag, ##__VA_ARGS__); \
        }                                                                                             \
    } while (0)

#define LOGW(tag, fmt, ...)                                                                             \
    do                                                                                                  \
    {                                                                                                   \
        if (_log_active)                                                                                \
        {                                                                                               \
            uint32_t ms = HAL_GetTick();                                                                \
            log_printf(tag, LOG_WARNING, LOG_FORMAT(W, fmt), ms / 1000, ms % 1000, tag, ##__VA_ARGS__); \
        }                                                                                               \
    } while (0)

#define LOGI(tag, fmt, ...)                                                                          \
    do                                                                                               \
    {                                                                                                \
        if (_log_active)                                                                             \
        {                                                                                            \
            uint32_t ms = HAL_GetTick();                                                             \
            log_printf(tag, LOG_INFO, LOG_FORMAT(I, fmt), ms / 1000, ms % 1000, tag, ##__VA_ARGS__); \
        }                                                                                            \
    } while (0)

#define LOGD(tag, fmt, ...)                                                                           \
    do                                                                                                \
    {                                                                                                 \
        if (_log_active)                                                                              \
        {                                                                                             \
            uint32_t ms = HAL_GetTick();                                                              \
            log_printf(tag, LOG_DEBUG, LOG_FORMAT(D, fmt), ms / 1000, ms % 1000, tag, ##__VA_ARGS__); \
        }                                                                                             \
    } while (0)

#define LOGV(tag, fmt, ...)                                                                             \
    do                                                                                                  \
    {                                                                                                   \
        if (_log_active)                                                                                \
        {                                                                                               \
            uint32_t ms = HAL_GetTick();                                                                \
            log_printf(tag, LOG_VERBOSE, LOG_FORMAT(V, fmt), ms / 1000, ms % 1000, tag, ##__VA_ARGS__); \
        }                                                                                               \
    } while (0)

/**
 * @brief Runtime macro to output message with no specified level.
 * 
 * @param format Format string.
 * @param ... Variable arguments.
 *
 * In essence, this macro bypasses log level checks and extra formatting.
 */
#define LOG(format, ...) printf(LOG_RESET_COLOUR format, ##__VA_ARGS__)

/**
 * @brief Assertion macro.
 *
 * If assertion fails, program enters forever loop.
 */
#define ASSERT(check)                                                           \
    do                                                                          \
    {                                                                           \
        if (!(check))                                                           \
        {                                                                       \
            uint32_t ms = HAL_GetTick();                                        \
            printf(ASSERTION_FORMAT, ms / 1000, ms % 1000, __FILE__, __LINE__); \
            while (1)                                                           \
            {                                                                   \
            }                                                                   \
        }                                                                       \
    } while (0)

#endif // _LOG_H_
