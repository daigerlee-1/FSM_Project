/*******************************************************************************
    1. C O P Y R I G H T
--------------------------------------------------------------------------------
    \par      copyright
    \verbatim
    Copyright (c) 2019-2024 by Mobase electronics. All rights reserved.
                This software is copyright protected and proprietary to
                Mobase electronics.
    \endverbatim
--------------------------------------------------------------------------------
    2. F I L E  D E S C R I P T I O N
--------------------------------------------------------------------------------
    \file   fsm_timer.h
    \brief  Declares the central 1 ms one-shot timer manager for FSM modules.
            Each timer uses a fixed ID and latches expiration until consumed by
            its owning feature.
********************************************************************************
|-------------------------------------------------------------------------------
|    3. A U T H O R   I D E N T I T Y
|-------------------------------------------------------------------------------
| Initials     Name                      Company
| --------     ---------------------     ---------------------------------------
| Ck           choi kyounghum            Mobaseelectronic software design team
| DL           lee juhwan(DaigerLee)     Mobaseelectronic software design team
|
|-------------------------------------------------------------------------------
|               R E V I S I O N   H I S T O R Y
|-------------------------------------------------------------------------------
| Date       Ver        Author  Description
| ---------  ---        ------  ------------------------------------------------
| 2025-03-10 000.01     DL      Creation
| 2026-10-15 000.02     DL      Standardized public header documentation
| 2026-10-21 000.03     DL      Added power voltage hysteresis timers
| 2026-10-22 000.04     DL      Added HU power-state timeout timer
| 2026-10-26 000.05     DL      Protected ISR and task shared timer state
| 2026-10-29 000.06     DL      Added LCAN NoCom retry timer
| 2026-10-29 000.07     DL      Grouped timer identifiers by feature
| 2026-11-13 000.08     DL      Removed redundant notification-period macro
********************************************************************************/
#ifndef FSM_TIMER_H
#define FSM_TIMER_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Fixed timer identifiers shared by the application features. */
typedef enum
{
    /* Power-management timers. */
    FSM_TIMER_POWER_HU_TIMEOUT = 0U,
    FSM_TIMER_POWER_ACC_HYSTERESIS,
    FSM_TIMER_POWER_BAT_HYSTERESIS,
    FSM_TIMER_POWER_LCAN_SLEEP_RETRY,

    /* Button-input timers. */
    FSM_TIMER_BUTTON_DEBOUNCE,

    /* LED-control timers. */
    FSM_TIMER_LED_RESET,
    FSM_TIMER_LED_BLINK,

    /* CAN communication timers. */
    FSM_TIMER_CAN_TX,

    /* Nonvolatile-memory timers. */
    FSM_TIMER_NVM_WRITE,

    /* Knob-wheel timers. */
    FSM_TIMER_KNOB_WHEEL,
    FSM_TIMER_KNOB_WHEEL_COUNT_WINDOW,
    FSM_TIMER_KNOB_WHEEL_INVALID_DELAY,

    /* Number of registered central timers; not a usable timer ID. */
    FSM_TIMER_MAX
} FSM_TimerIdType;

/** Mutable state of one table-based one-shot timer. */
typedef struct
{
    volatile uint32 counter;  /**< Remaining number of 1 ms ticks. */
    volatile boolean active;  /**< TRUE while the countdown is running. */
    volatile boolean expired; /**< Latched until consumed by IsExpired. */
} FSM_TimerContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** \brief Initializes every registered timer and enables the 1 ms time base. */
void FSM_Timer_Init(void);

/** \brief Advances all active timers by one 1 ms tick. */
void FSM_Timer_Notification(void);

/**
 * \brief Starts or restarts one timer.
 *
 * A zero-tick request or an invalid ID is rejected and leaves the timer as-is.
 */
void FSM_Timer_Start(FSM_TimerIdType timerId, uint32 ticks);

/** \brief Stops one timer and clears its pending expiration. */
void FSM_Timer_Stop(FSM_TimerIdType timerId);

/**
 * \brief Returns and clears the latched expiration state of one timer.
 * \return TRUE once after expiration; otherwise FALSE.
 */
boolean FSM_Timer_IsExpired(FSM_TimerIdType timerId);

#endif /* FSM_TIMER_H */
