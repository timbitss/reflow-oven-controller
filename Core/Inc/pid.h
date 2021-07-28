/**
 * @file pid.h
 * @author Timothy Nguyen
 * @brief PID Controller
 * @version 0.1
 * @date 2021-07-12
 * 
 *      Adapted from Philip Salmony's PID controller implementation: https://github.com/pms67/PID
 * 
 *      Integrator anti-windup method based on Bryan Douglas' PID video: https://www.youtube.com/watch?v=NVLXCwc8HzM
 * 
 */

#ifndef _PID_H_
#define _PID_H_

/* PID controller structure */
typedef struct
{
    float Kp; // Proportional gain.
    float Ki; // Integral gain.
    float Kd; // Derivative gain.

    float tau; // Derivative low-pass filter time constant
    float Ts;  // Sample time (s).

    float out_lim_max; // Output maximum saturation limit.
    float out_lim_min; // Output minimum saturation limit.

    /* Controller memory */
    float integral;         // Integral term.
    float derivative;       // Derivative term.
    float prev_error;       // Previous error, required for integrator.
    float prev_measurement; // Previous measurement, required for differentiator.

    /* Solely for data logging */
    float proportional;

    float out; // Controller output.
} PID_t;

/* PID configuration structure */
typedef struct
{
    /* Controller gains */
    float Kp;
    float Ki;
    float Kd;

    float tau;
    float Ts;

    float out_max;
    float out_min;
} PID_cfg_t;

/**
 * @brief Set/update PID controller parameters and erase controller memory.
 * 
 * @param[in/out] pid PID instance to initialize.
 * @param[in] PID_cfg PID configuration parameters.
 */
void PID_Init(PID_t *const pid, PID_cfg_t const *const pid_cfg);

/**
 * @brief Perform PID iteration.
 * 
 * @note  Setpoint and measurement arguments must have the same units.
 * @param pid PID structure containing controller parameter.
 * @param setpoint Setpoint value for current iteration.
 * @param measurement Measured value for current iteration.
 * @return float Output of PID calculation.
 */
float PID_Calculate(PID_t *const pid, float setpoint, float measurement);

/**
 * @brief Clear PID memory but retain controller parameters.
 * 
 * @param pid PID structure containing PID parameters and controller memory.
 */
void PID_Reset(PID_t *const pid);

#endif
