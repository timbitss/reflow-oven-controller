/**
 * @file reflow.h
 * @author Timothy Nguyen
 * @brief Reflow Oven Controller.
 * @version 0.1
 * @date 2021-07-22
 */

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "reflow.h"
#include "pid.h"
#include "active.h"
#include "log.h"
#include "cmd.h"
#include "cmsis_os.h"
#include "stm32l4xx.h"
#include "MAX31855K.h"

#define REFLOW_PROFILE_PHASES_CSV "RESET", "PREHEAT", "SOAK", "RAMPUP", "PEAK", "COOLDOWN"

/* Reflow oven states. */
typedef enum
{
    RESET_STATE,

    /* Reflow profile phases. */
    PREHEAT_STATE,
    SOAK_STATE,
    RAMPUP_STATE,
    PEAK_STATE,
    COOLDOWN_STATE,

    NUM_REFLOW_STATES,
    NUM_PROFILE_PHASES = NUM_REFLOW_STATES - 1
} Reflow_State;

/* Status code when returning from event handler. */
typedef enum
{
    TRAN_STATUS,    // State transition was taken.
    HANDLED_STATUS, // Event was handled but not state transition was taken.
    IGNORE_STATUS,  // Event was ignored.
    INIT_STATUS,    // Initial state transition was taken.
} Reflow_Status;

/* Reflow profile phase characteristic. */
typedef struct
{
    enum
    {
        REACHTEMP,      // Attain a specific temperature with maximum gradient.
        REACHTIME,      // Run profile element for specified time.
    } const phase_type; // REACHTEMP or REACHTIME

    uint32_t reach_temp;
    uint32_t reach_time;
} Reflow_Phase;

/* Reflow controller active object */
typedef struct
{
    Active reflow_base; // Inherited base Active object class.

    /* PWM attributes */
    TIM_HandleTypeDef *pwm_timer_handle;
    uint32_t pwm_channel;

    /* Timer instances */
    TimeEvent reflow_time_evt; // Time event for REACHTIME reflow phases.
    osTimerId_t pid_timer_id;  // 1/Ts Hz timer for PID calculations.

    /* Other variables */
    Reflow_State state;                                   // State variable for state machine.
    PID_t pid_params;                                     // PID parameters.
    float step_size;                                      // Temperature step size for REACHTIME phases (deg C / sample).
    float setpoint;                                       // Setpoint temperature.
    const Reflow_Phase reflow_phases[NUM_PROFILE_PHASES]; // Reflow phase characteristics.
} Reflow_Active;

/* Callback function prototype for event handler. */
typedef Reflow_Status (*ReflowAction)(Reflow_Active *const ao, Event const *const evt);

static void reflow_evt_handler(Reflow_Active *const ao, Event const *const evt); // Event handler.
static inline void displayPIDParams();                                           // Display PID parameters.
static inline void displayProfileParams();                                       // Display reflow profile phase parameters.
static inline void displayState();                                               // Display current state.
static uint32_t reflow_status_cmd(uint32_t argc, const char **argv);             // Display various reflow parameters and state.
static uint32_t reflow_start_cmd(uint32_t argc, const char **argv);              // Start reflow process command handler.
static uint32_t reflow_stop_cmd(uint32_t argc, const char **argv);               // Stop reflow process command handler.
static uint32_t reflow_set_cmd(uint32_t argc, const char **argv);                // Set PID parameters.
static void reflow_pid_iteration(void *argument);                                // Discrete PID controller iteration.
static inline bool readTemperature(float *const temp);                           // Read thermocouple temperature.

/* Reflow active object. */
static Reflow_Active reflow_ao = {
    .reflow_phases = {{.phase_type = REACHTEMP, .reach_temp = 100},                    // Pre-heat
                      {.phase_type = REACHTIME, .reach_temp = 150, .reach_time = 120}, // Soak
                      {.phase_type = REACHTEMP, .reach_temp = 215},                    // Ramp-up
                      {.phase_type = REACHTIME, .reach_temp = 215, .reach_time = 5},   // Peak
                      {.phase_type = REACHTEMP, .reach_temp = 35}}                     // Cool-down
};

/* Unique module tag for logging information */
static const char *TAG = "REFLOW";

/* Names of reflow profile phases as null-terminated string constants. */
static const char *reflow_names[NUM_REFLOW_STATES] = {REFLOW_PROFILE_PHASES_CSV};

/* Information about reflow commands. */
static const cmd_cmd_info reflow_cmd_infos[] = {
    {.cmd_name = "status",
     .cb = &reflow_status_cmd,
     .help = "Dump information about reflow oven controller."},
    {.cmd_name = "start",
     .cb = &reflow_start_cmd,
     .help = "Start reflow process."},
    {.cmd_name = "stop",
     .cb = &reflow_stop_cmd,
     .help = "Stop reflow process."},
    {.cmd_name = "set",
     .cb = &reflow_set_cmd,
     .help = "Set pid parameters (Kp, Ki, Kd, Tau)\r\nUsage: reflow set <param> <value> [<param2> <value2> ...] "}};

/* Client information for command module */
static cmd_client_info reflow_client_info = {.client_name = "reflow", // Client name (first command line token)
                                             .num_cmds = 4,
                                             .cmds = reflow_cmd_infos,
                                             .num_u16_pms = 0,
                                             .u16_pms = NULL,
                                             .u16_pm_names = NULL};

/* Stop reflow process event signal */
static const Event stop_evt = {.sig = STOP_REFLOW_SIG};

/*---------------------------------------------------------------------------*/
/* State machine facilities... */

static Reflow_Status Reflow_reset_INIT(Reflow_Active *const ao, Event const *const evt)
{
    LOGI(TAG, "Initializing reflow oven controller...");
    ao->state = RESET_STATE; // Redundant, but just in case.
    return INIT_STATUS;
}

static Reflow_Status Reflow_reset_ENTRY(Reflow_Active *const ao, Event const *const evt)
{
    /* Disable PWM output signal */
    LOGI(TAG, "Turning PWM off.");
    __HAL_TIM_SET_COMPARE(ao->pwm_timer_handle, ao->pwm_channel, 0);
    HAL_TIM_PWM_Stop(ao->pwm_timer_handle, ao->pwm_channel);

    /* Clear PID memory */
    PID_Reset(&ao->pid_params);

    /* Disarm timers */
    osTimerStop(ao->pid_timer_id);
    TimeEvent_disarm(&ao->reflow_time_evt);

    LOGI(TAG, "Reflow oven controller initialized.");
    LOGI(TAG, "Enter command \"reflow start\" to start reflow process.");
    return HANDLED_STATUS;
}

static Reflow_Status Reflow_preheat_ENTRY(Reflow_Active *const ao, Event const *const evt)
{
    HAL_TIM_PWM_Start(ao->pwm_timer_handle, ao->pwm_channel);
    ao->setpoint = (float)ao->reflow_phases[PREHEAT_STATE - 1].reach_temp;
    osTimerStart(ao->pid_timer_id, (uint32_t)(ao->pid_params.Ts * 1000));
    return HANDLED_STATUS;
}

static Reflow_Status Reflow_soak_ENTRY(Reflow_Active *const ao, Event const *const evt)
{
    /* Set step size for slowest temperature rise. */
    ao->step_size = (float)(ao->reflow_phases[SOAK_STATE - 1].reach_temp - ao->reflow_phases[PREHEAT_STATE - 1].reach_temp) /
                    (ao->reflow_phases[SOAK_STATE - 1].reach_time * (1 / ao->pid_params.Ts));
    TimeEvent_arm(&ao->reflow_time_evt, ao->reflow_phases[SOAK_STATE - 1].reach_time, 0);
    return HANDLED_STATUS;
}

static Reflow_Status Reflow_rampup_ENTRY(Reflow_Active *const ao, Event const *const evt)
{
    ao->setpoint = (float)ao->reflow_phases[RAMPUP_STATE - 1].reach_temp;
    return HANDLED_STATUS;
}

static Reflow_Status Reflow_peak_ENTRY(Reflow_Active *const ao, Event const *const evt)
{
    ao->step_size = 0;
    TimeEvent_arm(&ao->reflow_time_evt, ao->reflow_phases[PEAK_STATE - 1].reach_time, 0);
    return HANDLED_STATUS;
}

static Reflow_Status Reflow_cooldown_ENTRY(Reflow_Active *const ao, Event const *const evt)
{
    ao->setpoint = (float)ao->reflow_phases[COOLDOWN_STATE - 1].reach_temp;
    return HANDLED_STATUS;
}

static Reflow_Status Reflow_reset_START(Reflow_Active *const ao, Event const *const evt)
{
    /* Check that oven temperature has cooled down. */
    float current_temp = 0;
    if (readTemperature(&current_temp) != true)
    {
        LOGW(TAG, "MAX31855K Read Error, unable to start reflow process.");
        return HANDLED_STATUS;
    }
    else if ((uint32_t)current_temp > ao->reflow_phases[COOLDOWN_STATE - 1].reach_temp) // Subtract 1 due to RESET_STATE.)
    {
        LOGW(TAG, "Oven temperature must cool to below %lu before starting another run.",
             ao->reflow_phases[COOLDOWN_STATE - 1].reach_temp);
        return HANDLED_STATUS;
    }
    else
    {
        LOG("Starting reflow process\r\n");
        LOGI(TAG, "Entering pre-heat phase.");
        ao->state = PREHEAT_STATE;
        return TRAN_STATUS;
    }
}

static Reflow_Status Reflow_preheat_REACHTEMP(Reflow_Active *const ao, Event const *const evt)
{
    LOGI(TAG, "Entering soak phase.");
    ao->state = SOAK_STATE;
    return TRAN_STATUS;
}

static Reflow_Status Reflow_soak_REACHTIME(Reflow_Active *const ao, Event const *const evt)
{
    LOGI(TAG, "Entering ramp-up phase.");
    ao->state = RAMPUP_STATE;
    return TRAN_STATUS;
}

static Reflow_Status Reflow_rampup_REACHTEMP(Reflow_Active *const ao, Event const *const evt)
{
    LOGI(TAG, "Entering peak phase.");
    ao->state = PEAK_STATE;
    return TRAN_STATUS;
}

static Reflow_Status Reflow_peak_REACHTIME(Reflow_Active *const ao, Event const *const evt)
{
    LOGI(TAG, "Entering cool-down phase.");
    ao->state = COOLDOWN_STATE;
    return TRAN_STATUS;
}

static Reflow_Status Reflow_cooldown_REACHTEMP(Reflow_Active *const ao, Event const *const evt)
{
    LOGI(TAG, "Reflow process completed!");
    ao->state = RESET_STATE;
    return TRAN_STATUS;
}

static Reflow_Status Reflow_STOP(Reflow_Active *const ao, Event const *const evt)
{
    LOG("Reflow process stopped\r\n");
    osTimerStop(ao->pid_timer_id);
    ao->state = RESET_STATE; // Transition to RESET state.
    return TRAN_STATUS;
}

static Reflow_Status Reflow_ignore(Reflow_Active *const ao, Event const *const evt)
{
    return IGNORE_STATUS;
}

/* State machine table */
static const ReflowAction Reflow_state_table[NUM_REFLOW_STATES][NUM_REFLOW_SIGS] = {
    /*              INIT_SIG,    ENTRY_SIG,    START_REFLOW_SIG,    REACH_TIME_SIG,    REACH_TEMP_SIG,    STOP_REFLOW_SIG */
    /* RESET  	*/ {Reflow_reset_INIT, Reflow_reset_ENTRY, Reflow_reset_START, Reflow_ignore, Reflow_ignore, Reflow_ignore},
    /* PREHEAT 	*/ {Reflow_ignore, Reflow_preheat_ENTRY, Reflow_ignore, Reflow_ignore, Reflow_preheat_REACHTEMP, Reflow_STOP},
    /* SOAK 	*/ {Reflow_ignore, Reflow_soak_ENTRY, Reflow_ignore, Reflow_soak_REACHTIME, Reflow_ignore, Reflow_STOP},
    /* RAMPUP 	*/ {Reflow_ignore, Reflow_rampup_ENTRY, Reflow_ignore, Reflow_ignore, Reflow_rampup_REACHTEMP, Reflow_STOP},
    /* PEAK 	*/ {Reflow_ignore, Reflow_peak_ENTRY, Reflow_ignore, Reflow_peak_REACHTIME, Reflow_ignore, Reflow_STOP},
    /* COOLDOWN */ {Reflow_ignore, Reflow_cooldown_ENTRY, Reflow_ignore, Reflow_ignore, Reflow_cooldown_REACHTEMP, Reflow_STOP}};

void reflow_init(Reflow_cfg_t const *const reflow_cfg)
{
    /* Call active object constructor */
    Active_ctor((Active *)&reflow_ao, (EventHandler)reflow_evt_handler);

    /* Register PWM timer and enable preload register */
    reflow_ao.pwm_timer_handle = reflow_cfg->pwm_timer_handle;
    reflow_ao.pwm_channel = reflow_cfg->pwm_channel;
    __HAL_TIM_ENABLE_OCxPRELOAD(reflow_ao.pwm_timer_handle, reflow_ao.pwm_channel);

    /* Setup PID parameters */
    static const PID_cfg_t reflow_pid_cfg = {.Kp = KP_INIT,
                                             .Ki = KI_INIT,
                                             .Kd = KD_INIT,
                                             .tau = TAU_INIT,
                                             .Ts = TS_INIT,
                                             .out_max = OUT_MAX_INIT,
                                             .out_min = OUT_MIN_INIT};
    PID_Init(&reflow_ao.pid_params, &reflow_pid_cfg);

    /* Initialize timer instances. */
    TimeEvent_ctor(&reflow_ao.reflow_time_evt, REACH_TIME_SIG, (Active *)&reflow_ao);
    reflow_ao.pid_timer_id = osTimerNew(reflow_pid_iteration, osTimerPeriodic, NULL, NULL);

    /* Register reflow commands */
    cmd_register(&reflow_client_info);

    /* Initialize thermocouple IC */
    MAX31855K_Init(&reflow_cfg->max_cfg);

    LOGI(TAG, "Initialized reflow module.");
}

void reflow_start()
{
    osThreadAttr_t reflow_thread_attr = {.stack_size = REFLOW_THREAD_STACK_SZ};
    Active_start((Active *)&reflow_ao, &reflow_thread_attr, 5, NULL);
}

/**
 * @brief Perform PID iteration.
 */
static void reflow_pid_iteration(void *argument)
{
    /* Read temperature */
    float temp_reading = 0;
    bool status = readTemperature(&temp_reading);
    if (status == false)
    {
        LOGE(TAG, "Could not read temperature, aborting reflow process.");
        Active_post(&reflow_ao.reflow_base, &stop_evt);
    }

    /* Check if temperature reached intended temperature of REACHTEMP phases.
	 * If so, send REACHTEMP signal to reflow active object.
	 */
    if (reflow_ao.reflow_phases[reflow_ao.state - 1].phase_type == REACHTEMP)
    {
        uint32_t reach_temp = reflow_ao.reflow_phases[reflow_ao.state - 1].reach_temp;
        /* Give some leeway. */
        if (reach_temp > (uint32_t)temp_reading - 2U && reach_temp < (uint32_t)temp_reading + 2U)
        {
            static const Event reachtemp_evt = {.sig = REACH_TEMP_SIG};
            Active_post(&reflow_ao.reflow_base, &reachtemp_evt);
        }
    }
    else
    {
        /* Currently in a REACHTIME phase.
	     * Update setpoint by a step to smooth out temperature change.
	     */
        reflow_ao.setpoint += reflow_ao.step_size;
    }

    /* Acquire new PWM output signal through feedback control. */
    float pwm_value = PID_Calculate(&reflow_ao.pid_params, reflow_ao.setpoint, temp_reading);

    /* Set PWM signal */
    __HAL_TIM_SET_COMPARE(reflow_ao.pwm_timer_handle, reflow_ao.pwm_channel, (uint16_t)pwm_value);

    LOGI(TAG, "%s %.2f %.2f %.2f %.2f %.2f %.2f",
         reflow_names[reflow_ao.state],
         reflow_ao.setpoint,
         temp_reading,
         reflow_ao.pid_params.proportional,
         reflow_ao.pid_params.integral,
         reflow_ao.pid_params.derivative,
         pwm_value);
}

/**
 * @brief Display PID, profile parameters, or both to user.
 */
static uint32_t reflow_status_cmd(uint32_t argc, const char **argv)
{
    displayPIDParams();
    displayProfileParams();
    displayState();
    float oven_temp = 0;
    if (readTemperature(&oven_temp))
    {
        LOG("Oven temperature: %.2f\r\n", oven_temp);
    }
    else
    {
        LOG("Oven temperature read error: %s\r\n", MAX31855K_Err_Str());
    }
    return 0;
}

static uint32_t reflow_start_cmd(uint32_t argc, const char **argv)
{
    static const Event start_evt = {.sig = START_REFLOW_SIG};
    Active_post(&reflow_ao.reflow_base, &start_evt);
    LOG("Posted START signal to reflow active object.\r\n");
    return 0;
}

static uint32_t reflow_stop_cmd(uint32_t argc, const char **argv)
{
    Active_post(&reflow_ao.reflow_base, &stop_evt);
    LOG("Posted STOP signal to reflow active object.\r\n");
    return 0;
}

static uint32_t reflow_set_cmd(uint32_t argc, const char **argv)
{
    if (argc % 2 != 0 || argc == 0)
    {
        LOG("Invalid number of arguments\r\n");
        return -1;
    }

    /* Iterate through <param>,<value> pairs */
    for (uint8_t i = 0; i < argc; i += 2)
    {
        const char *param = argv[i];
        char **end_ptr;
        uint32_t val = strtoul(argv[i + 1], end_ptr, 0);
        if (strcasecmp(param, "Kp") == 0)
        {

            reflow_ao.pid_params.Kp = (float)val;
            LOG("Updated Kp to %.2f\r\n", reflow_ao.pid_params.Kp);
        }
        else if (strcasecmp(param, "Kd") == 0)
        {

            reflow_ao.pid_params.Kd = (float)val;
            LOG("Updated Kd to %.2f\r\n", reflow_ao.pid_params.Kd);
        }
        else if (strcasecmp(param, "Ki") == 0)
        {

            reflow_ao.pid_params.Ki = (float)val;
            LOG("Updated Ki to %.2f\r\n", reflow_ao.pid_params.Ki);
        }
        else if (strcasecmp(param, "Tau") == 0)
        {
            reflow_ao.pid_params.tau = (float)val;
            LOG("Updated tau to %.2f\r\n", reflow_ao.pid_params.tau);
        }
        else
        {
            LOG("Unrecognizable PID parameter: %s\r\n", param);
            return -1;
        }
    }

    return 0;
}

static void reflow_evt_handler(Reflow_Active *const ao, Event const *const evt)
{
    /* Use state table to handle events */
    ASSERT((ao->state < NUM_REFLOW_STATES) && (evt->sig < NUM_REFLOW_SIGS));
    Reflow_Status stat = Reflow_state_table[ao->state][evt->sig](ao, evt);

    /**
	 * Execute entry action of current state if
	 * state transition was taken.
	 */
    if (stat == TRAN_STATUS)
    {
        ASSERT(ao->state < NUM_REFLOW_STATES);
        Reflow_state_table[ao->state][ENTRY_SIG](ao, (Event *)0);
    }
    else if (stat == INIT_STATUS)
    {
        /* Execute entry action of initial state. */
        Reflow_state_table[ao->state][ENTRY_SIG](ao, (Event *)0);
    }
}

static inline void displayPIDParams()
{
    LOG("Kp: %.2f\tKi: %.2f\tKd: %.2f\tTau: %.2f\r\n"
        "Sampling Period: %.2f s\tMax Limit: %.2f\tMin Limit: %.2f\r\n",
        reflow_ao.pid_params.Kp, reflow_ao.pid_params.Ki, reflow_ao.pid_params.Kd,
        reflow_ao.pid_params.tau, reflow_ao.pid_params.Ts,
        reflow_ao.pid_params.out_lim_max, reflow_ao.pid_params.out_lim_min);
}

static inline void displayProfileParams()
{
    for (uint8_t i = 0; i < NUM_PROFILE_PHASES; i++)
    {
        LOG("Phase: %s\tType: %s\tReach Temp: %lu deg C\tReach Time: %lu s\r\n",
            reflow_names[i + 1], reflow_ao.reflow_phases[i].phase_type == REACHTEMP ? "REACHTEMP" : "REACHTIME",
            reflow_ao.reflow_phases[i].reach_temp,
            reflow_ao.reflow_phases[i].reach_time);
    }
}

static inline void displayState()
{
    LOG("Current state: %s\r\n", reflow_names[reflow_ao.state]);
}

/**
 * @brief Read thermocouple temperature.
 *
 * @param[in/out] temp Temperature reading if return value is true, unmodified otherwise.
 *
 * @return true if successful read, false otherwise.
 */
static inline bool readTemperature(float *const temp)
{
    MAX31855K_err_t err = MAX31855K_RxBlocking();
    if (err)
    {
        return false;
    }
    else
    {
        *temp = MAX31855K_Get_HJ();
        return true;
    }
}
