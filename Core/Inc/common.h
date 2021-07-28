/**
 * @file common.h
 * @author Timothy Nguyen
 * @brief Common type definitions and macros.
 * @version 0.1
 * @date 2021-07-14
 */

#ifndef _COMMON_H_
#define _COMMON_H_

/**
 * @brief Common error type definition for modules.
 */
typedef enum
{
    MOD_OK, // Successful return value.

    MOD_ERR,              // Generic error value.
    MOD_ERR_ARG,          // Bad argument(s) passed to function.
    MOD_ERR_RESOURCE,     // Insufficient memory.
    MOD_ERR_BAD_CMD,      // Bad command.
    MOD_ERR_BUF_OVERRUN,  // Buffer overrun.
    MOD_ERR_NOT_INIT,     // Module has not been initialized.
    MOD_ERR_BAD_INSTANCE, // Module instance has invalid values.
    MOD_ERR_PERIPH,       // Peripheral disabled.
    MOD_ERR_TIMEOUT,      // OS blocking function timed out.

    MOD_DID_NOTHING, // Function returned immediately without doing anything.
} mod_err_t;

/* Get number of elements in an array */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Increment a uint16_t, saturating at the maximum value.
#define INC_SAT_U16(a)                      \
    do                                      \
    {                                       \
        (a) += ((a) == UINT16_MAX ? 0 : 1); \
    } while (0)

/* Clamp a numeric value between a lower and upper limit, inclusive. */
#define CLAMP(a, low, high) ((a) <= (low) ? (low) : ((a) > (high) ? (high) : (a)))

#endif
