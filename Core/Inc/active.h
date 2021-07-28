/**
 * @file active.h
 * @author Timothy Nguyen
 * @brief Generic Active object framework.
 * @version 0.1
 * @date 2021-07-20
 */

#ifndef _ACTIVE_H_
#define _ACTIVE_H_

#include <stdint.h>
#include "common.h"
#include "cmsis_os.h"

/**
 * @brief Generic signal type definition.
 * 
 * Modules may include their own enumerated signals.
 */
typedef int32_t Signal;

/* Reserved signals */
enum ReservedSignals
{
    INIT_SIG,  // Dispatched to AO before entering event-loop
    ENTRY_SIG, // Trigger action upon entry into state.
    USER_SIG   // First signal available to the users
};

/**
 * @brief Event base class.
 *
 * Modules may inherit base class and add relevant private event parameters.
 */
typedef struct
{
    Signal sig; // Event signal.
} Event;

/* Forward declaration */
typedef struct Active Active;

/**
 * @brief Time event class.
 * 
 * Timeout event is posted to specified active object 
 * once timeout value reaches 0.
 */
typedef struct
{
    Event base; // Inherit base Event class.

    Active *ao;       // Active object that requested the time event.
    uint32_t timeout; // Timeout counter in ms; 0 means not armed.
    uint32_t reload;  // Reload value for periodic time events, 0 means one-shot.
} TimeEvent;

/* Event handler function pointer typedef. */
typedef void (*EventHandler)(Active *const ao, Event const *const evt);

/**
 * @brief Active object base class.
 *
 * Modules may inherit base class and add relevant private data fields.
 */
struct Active
{
    /* Private variables */
    osThreadId_t thread_id;      // Event loop thread ID.
    osMessageQueueId_t queue_id; // Event message queue ID.

    /* Virtual functions */
    EventHandler evt_handler; // Event handler function.
};

/**
 * @brief Active object constructor.
 * 
 * @param[in/out] ao Base active object.
 * @param[in] evt_handler Event handler function.
 * 
 * @return MOD_OK if successful, a "MOD_ERR" value otherwise.
 */
mod_err_t Active_ctor(Active *const ao, EventHandler evt_handler);

/**
 * @brief Start active object's thread and message queue.
 * 
 * @param[in/out] ao Base active object.
 * @param[in] thread_attr Thread attributes (NULL for default).
 * @param[in] msg_count Maximum number of messages/events in queue.
 * @param[in] queue_attr Queue attributes (NULL for default).
 * 
 * @return MOD_OK if successful, a "MOD_ERR" value otherwise.
 * 
 * @note Function does not start thread scheduler.
 */
mod_err_t Active_start(Active *const ao,
                       const osThreadAttr_t *const thread_attr,
                       uint32_t msg_count,
                       const osMessageQueueAttr_t *const queue_attr);

/**
 * @brief Post message to active object (non-blocking).
 * 
 * @param ao Base active object to post message to.
 * @param evt Event information.
 * 
 * @return MOD_OK if successful, MOD_ERR_TIMEOUT if queue is full.
 * 
 * @note Race condition may occur if event object is modified while event is being processed.
 */
mod_err_t Active_post(Active *const ao, Event const *const evt);

/**
 * @brief Time event constructor.
 * 
 * @param[in/out] time_evt Time_evt instance.
 * @param[in] sig User-defined signal which is posted when timeout value reaches 0.
 * @param[in] ao Active object instance registered to timer event.
 * 
 * Timer event is initially disarmed.
 */
void TimeEvent_ctor(TimeEvent *const time_evt, Signal sig, Active *ao);

/**
 * @brief Arm specific time event instance.
 * 
 * @param[in/out] time_evt Timer event instance.
 * @param[in] timeout Timeout value (ms).
 * @param[in] reload Auto-reload value (0 for one-shot).
 */
void TimeEvent_arm(TimeEvent *const time_evt, uint32_t timeout, uint32_t reload);

/**
 * @brief Disarm specific time event instance.
 * 
 * @param[in/out] time_evt Timer event instance.
 */
void TimeEvent_disarm(TimeEvent *const time_evt);

#endif
