/**
 * @file reflow.h
 * @author Timothy Nguyen
 * @brief Reflow Oven Controller
 * @version 0.1
 * @date 2021-07-22
 */

#ifndef __REFLOW_H__
#define __REFLOW_H__

#include <stdint.h>

#include "active.h"
#include "stm32l4xx.h"
#include "MAX31855K.h"

/* Configuration parameters */
#define REFLOW_THREAD_STACK_SZ 1024 * 2

#define KP_INIT 10.0f        // Kp gain.
#define KI_INIT 0.0f         // Ki gain.
#define KD_INIT 0.0f         // Kd gain.
#define TAU_INIT 1.0f        // Low-pass filter time constant.
#define TS_INIT 0.5f         // Sampling period (s).
#define OUT_MAX_INIT 4095.0f // Maximum output saturation limit.
#define OUT_MIN_INIT 0.0f    // Minimum output saturation limit.

/* Reflow controller signals. */
enum ReflowSignal
{
    START_REFLOW_SIG = USER_SIG, // Start reflow process.
    REACH_TIME_SIG,              // Timeout event.
    REACH_TEMP_SIG,              // Reached specific temperature.
    STOP_REFLOW_SIG,             // Stop reflow process.

    NUM_REFLOW_SIGS
};

/* Reflow oven controller configuration structure */
typedef struct
{
    TIM_HandleTypeDef *pwm_timer_handle; // PWM Timer handle.
    uint32_t pwm_channel;                // PWM Timer channel.
    MAX31855K_cfg_t max_cfg;             // MAX31855K Thermocouple IC configuration structure.
} Reflow_cfg_t;

/**
 * @brief Initialize reflow oven controller.
 *
 * @param reflow_cfg Reflow oven configuration parameters.
 *
 * @note Make sure that timer period is 4095 ticks or 12-bit PWM resolution
 * 	     with PWM frequency of around 2 Hz.
 */
void reflow_init(Reflow_cfg_t const *const reflow_cfg);

/**
 * @brief Start reflow oven active object instance.
 *
 * This function does not start the scheduler.
 */
void reflow_start();

#endif
