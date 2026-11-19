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
    \file   fsm_timer.c
    \brief  Implements the central fixed-table 1 ms one-shot timer manager.
            Expiration is latched until consumed by the owning FSM module.
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
| 2026-10-15 000.02     DL      Standardized C source documentation
| 2026-10-26 000.03     DL      Protected ISR and task shared timer state
| 2026-11-13 000.04     DL      Removed redundant notification-rate helper
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "fsm_timer.h"

#include <Os.h>

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Protects task-side timer access from the 1 ms GPT interrupt. */
#define FSM_TIMER_ENTER_CRITICAL() SuspendAllInterrupts()
#define FSM_TIMER_EXIT_CRITICAL()  ResumeAllInterrupts()

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static boolean FSM_Timer_IsValidId(FSM_TimerIdType timerId);
static void FSM_Timer_Tick(void);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Runtime storage indexed by FSM_TimerIdType. */
static FSM_TimerContextType Fsm_Timer[FSM_TIMER_MAX];

/** Enables timer updates after successful timer-manager initialization. */
static volatile boolean FSM_TimerEnabled = FALSE;

/*******************************************************************************
    9. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/* No private constants or configuration tables are defined by this module. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Advances all timers when the configured 1 ms time base is enabled. */
void FSM_Timer_Notification(void)
{
    if (FSM_TimerEnabled == TRUE)
    {
        FSM_Timer_Tick();
    }
    else
    {
        /* Ignore notifications received before timer initialization. */
    }
}

/** \brief Clears every timer and enables the supported 1 ms time base. */
void FSM_Timer_Init(void)
{
    uint8 index;

    FSM_TIMER_ENTER_CRITICAL();

    FSM_TimerEnabled = FALSE;

    for (index = (uint8)0U; index < (uint8)FSM_TIMER_MAX; index++)
    {
        Fsm_Timer[index].counter = (uint32)0U;
        Fsm_Timer[index].active = FALSE;
        Fsm_Timer[index].expired = FALSE;
    }

    FSM_TimerEnabled = TRUE;

    FSM_TIMER_EXIT_CRITICAL();
}

/** \brief Starts or restarts one valid timer with a nonzero tick count. */
void FSM_Timer_Start(FSM_TimerIdType timerId, uint32 ticks)
{
    if ((FSM_Timer_IsValidId(timerId) == TRUE) &&
        (ticks > (uint32)0U))
    {
        FSM_TIMER_ENTER_CRITICAL();

        Fsm_Timer[(uint8)timerId].active = FALSE;
        Fsm_Timer[(uint8)timerId].counter = ticks;
        Fsm_Timer[(uint8)timerId].expired = FALSE;
        Fsm_Timer[(uint8)timerId].active = TRUE;

        FSM_TIMER_EXIT_CRITICAL();
    }
    else
    {
        /* Invalid and zero-tick requests do not alter timer state. */
    }
}

/** \brief Stops one timer and clears its pending expiration. */
void FSM_Timer_Stop(FSM_TimerIdType timerId)
{
    if (FSM_Timer_IsValidId(timerId) == TRUE)
    {
        FSM_TIMER_ENTER_CRITICAL();

        Fsm_Timer[(uint8)timerId].active = FALSE;
        Fsm_Timer[(uint8)timerId].counter = (uint32)0U;
        Fsm_Timer[(uint8)timerId].expired = FALSE;

        FSM_TIMER_EXIT_CRITICAL();
    }
    else
    {
        /* Ignore an invalid timer ID. */
    }
}

/** \brief Returns and clears the latched expiration of one timer. */
boolean FSM_Timer_IsExpired(FSM_TimerIdType timerId)
{
    boolean result = FALSE;

    if (FSM_Timer_IsValidId(timerId) == TRUE)
    {
        FSM_TIMER_ENTER_CRITICAL();

        result = Fsm_Timer[(uint8)timerId].expired;
        Fsm_Timer[(uint8)timerId].expired = FALSE;

        FSM_TIMER_EXIT_CRITICAL();
    }
    else
    {
        result = FALSE;
    }

    return result;
}

/** \brief Decrements each active timer and latches zero-count expiration. */
static void FSM_Timer_Tick(void)
{
    uint8 index;

    for (index = (uint8)0U; index < (uint8)FSM_TIMER_MAX; index++)
    {
        if (Fsm_Timer[index].active == TRUE)
        {
            if (Fsm_Timer[index].counter > (uint32)0U)
            {
                Fsm_Timer[index].counter--;
            }
            else
            {
                /* Recover a defensive active-with-zero inconsistency. */
            }

            /* Checking after decrement makes a one-tick timer expire at 1 ms. */
            if (Fsm_Timer[index].counter == (uint32)0U)
            {
                Fsm_Timer[index].active = FALSE;
                Fsm_Timer[index].expired = TRUE;
            }
            else
            {
                /* Timer remains active. */
            }
        }
        else
        {
            /* Timer is inactive. */
        }
    }
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/**
 * \brief Checks whether a timer identifier indexes the static timer table.
 *
 * \return TRUE for a valid identifier; otherwise FALSE.
 */
static boolean FSM_Timer_IsValidId(FSM_TimerIdType timerId)
{
    boolean result;

    if ((uint32)timerId < (uint32)FSM_TIMER_MAX)
    {
        result = TRUE;
    }
    else
    {
        result = FALSE;
    }

    return result;
}
