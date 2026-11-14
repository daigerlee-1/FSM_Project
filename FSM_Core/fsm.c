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
    \file   fsm.c
    \brief  Implements generic FSM initialization, event dispatch, and cyclic
            state execution for table-based application state machines.
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
| 2026-11-14 000.03     DL      Added execution guard and reentry diagnostics
| 2026-11-14 000.04     DL      Deactivate FSM after invalid initialization
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "fsm.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Maximum retained reentry count; further rejections do not wrap it. */
#define FSM_REENTRY_REJECT_COUNT_MAX ((uint16)65535U)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/** \brief Acquires execution ownership or records a rejected reentry. */
static uint8 Fsm_TryEnterExecution(Fsm_ContextType * const context);

/** \brief Processes one event while the caller owns execution. */
static void Fsm_DispatchInternal(Fsm_ContextType * const context);

/** \brief Performs structural checks required before an FSM can run. */
static uint8 Fsm_IsConfigValid(const Fsm_ConfigType * const config);

/** \brief Checks every state-operation callback in an FSM configuration. */
static uint8 Fsm_AreStateOperationsValid(
    const Fsm_ConfigType * const config);

/** \brief Checks every transition row in an FSM configuration. */
static uint8 Fsm_AreTransitionsValid(
    const Fsm_ConfigType * const config);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/* No private variables are defined by this module. */

/*******************************************************************************
    9. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/* No private constants or configuration tables are defined by this module. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes an FSM context and enters its initial state. */
Std_ReturnType Fsm_Init(Fsm_ContextType * const context,
                        const Fsm_ConfigType * const config)
{
    Std_ReturnType ret = STD_NOT_OK;

    if (Fsm_TryEnterExecution(context) == STD_TRUE)
    {
        if (Fsm_IsConfigValid(config) == STD_TRUE)
        {
            /* eventCount is the sentinel meaning "no event handled". */
            context->config = config;
            context->state = config->initialState;
            context->lastEvent = config->eventCount;
            context->initialized = STD_TRUE;
            FsmQueue_Init(&context->eventQueue);

            /* Entry may post events, but cannot reenter FSM execution. */
            config->stateOperations[context->state].entry();
            ret = STD_OK;
        }
        else
        {
            /* Fail closed without invoking callbacks from the old config. */
            context->initialized = STD_FALSE;
            context->config = NULL_PTR;
            context->state = (Fsm_StateType)0U;
            context->lastEvent = (Fsm_EventType)0U;
            FsmQueue_Init(&context->eventQueue);
        }

        context->executing = STD_FALSE;
    }
    else
    {
        /* Reject null contexts and reentry without modifying FSM data. */
    }

    return ret;
}

/** \brief Validates and queues one event for an FSM instance. */
Std_ReturnType Fsm_PostEvent(Fsm_ContextType * const context,
                             Fsm_EventType event)
{
    Std_ReturnType ret = STD_NOT_OK;

    if ((context != NULL_PTR) &&
        (context->initialized == STD_TRUE) &&
        (context->config != NULL_PTR) &&
        (event < context->config->eventCount))
    {
        /* Queue overflow is returned to the event producer as STD_NOT_OK. */
        ret = FsmQueue_Push(&context->eventQueue, (FsmQueue_ValueType)event);
    }
    else
    {
        /* Reject invalid contexts and events. */
    }

    return ret;
}

/** \brief Removes and processes at most one queued event. */
void Fsm_Dispatch(Fsm_ContextType * const context)
{
    if (Fsm_TryEnterExecution(context) == STD_TRUE)
    {
        Fsm_DispatchInternal(context);
        context->executing = STD_FALSE;
    }
    else
    {
        /* A rejected call must not consume an event or release ownership. */
    }
}

/** \brief Executes the periodic operation of the active state. */
void Fsm_Do(Fsm_ContextType * const context)
{
    if (Fsm_TryEnterExecution(context) == STD_TRUE)
    {
        if ((context->initialized == STD_TRUE) &&
            (context->config != NULL_PTR) &&
            (context->config->stateOperations != NULL_PTR) &&
            (context->state < context->config->stateCount) &&
            (context->config->stateOperations[context->state].doAction !=
             (Fsm_StateFuncType)0))
        {
            context->config->stateOperations[context->state].doAction();
        }
        else
        {
            /* Ignore calls made before successful initialization. */
        }

        context->executing = STD_FALSE;
    }
    else
    {
        /* A rejected call must not execute a callback or release ownership. */
    }
}

/** \brief Returns the current state of a valid FSM context. */
Fsm_StateType Fsm_GetState(const Fsm_ContextType * const context)
{
    Fsm_StateType state = (Fsm_StateType)0U;

    if ((context != NULL_PTR) && (context->initialized == STD_TRUE))
    {
        state = context->state;
    }
    else
    {
        /* Return the zero state for an invalid context. */
    }

    return state;
}

/** \brief Returns the most recently dequeued event. */
Fsm_EventType Fsm_GetLastEvent(const Fsm_ContextType * const context)
{
    Fsm_EventType event = (Fsm_EventType)0U;

    if ((context != NULL_PTR) && (context->initialized == STD_TRUE))
    {
        event = context->lastEvent;
    }
    else
    {
        /* Return zero for an invalid context. */
    }

    return event;
}

/** \brief Returns the retained, saturating reentry rejection count. */
uint16 Fsm_GetReentryRejectCount(const Fsm_ContextType * const context)
{
    uint16 count = (uint16)0U;

    if (context != NULL_PTR)
    {
        count = context->reentryRejectCount;
    }
    else
    {
        /* A null context has no diagnostics to report. */
    }

    return count;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Serializes callback-driven execution in one cooperative context. */
static uint8 Fsm_TryEnterExecution(Fsm_ContextType * const context)
{
    uint8 ret = STD_FALSE;

    if (context != NULL_PTR)
    {
        if (context->executing == STD_FALSE)
        {
            context->executing = STD_TRUE;
            ret = STD_TRUE;
        }
        else
        {
            if (context->reentryRejectCount < FSM_REENTRY_REJECT_COUNT_MAX)
            {
                context->reentryRejectCount++;
            }
            else
            {
                /* Preserve saturation so repeated faults remain visible. */
            }
        }
    }
    else
    {
        /* A null context cannot acquire execution ownership. */
    }

    return ret;
}

/** \brief Removes and processes at most one event under execution ownership. */
static void Fsm_DispatchInternal(Fsm_ContextType * const context)
{
    FsmQueue_ValueType queuedEvent = (FsmQueue_ValueType)0U;
    uint16 index;
    uint8 transitionFound = STD_FALSE;

    if ((context != NULL_PTR) &&
        (context->initialized == STD_TRUE) &&
        (context->config != NULL_PTR) &&
        (context->config->stateOperations != NULL_PTR) &&
        (context->config->transitionTable != NULL_PTR) &&
        (context->state < context->config->stateCount))
    {
        /* The dispatch contract permits consumption of one event per call. */
        if (FsmQueue_Pop(&context->eventQueue, &queuedEvent) == STD_OK)
        {
            context->lastEvent = (Fsm_EventType)queuedEvent;

            /* Table order defines priority when duplicate rows are present. */
            for (index = (uint16)0U;
                 index < context->config->transitionCount;
                 index++)
            {
                const Fsm_TransitionType * transition =
                    &context->config->transitionTable[index];

                if ((transition->currentState == context->state) &&
                    (transition->event == (Fsm_EventType)queuedEvent))
                {
                    transitionFound = STD_TRUE;

                    if ((transition->nextState <
                         context->config->stateCount) &&
                        (transition->guard != (Fsm_GuardFuncType)0) &&
                        (transition->action != (Fsm_ActionFuncType)0) &&
                        (context->config->stateOperations[
                            context->state].exit != (Fsm_StateFuncType)0) &&
                        (context->config->stateOperations[
                            transition->nextState].entry !=
                            (Fsm_StateFuncType)0) &&
                        (transition->guard() == STD_TRUE))
                    {
                        /* Required transition order: exit, action, update, entry. */
                        context->config->stateOperations[context->state].exit();
                        transition->action();
                        context->state = transition->nextState;
                        context->config->stateOperations[context->state].entry();
                    }
                    else
                    {
                        /* A blocked or invalid transition consumes the event. */
                    }
                    break;
                }
                else
                {
                    /* Continue searching the configured transition table. */
                }
            }

            if (transitionFound == STD_FALSE)
            {
                /* An unmatched event is consumed and intentionally ignored. */
            }
            else
            {
                /* The event was handled or blocked by its guard. */
            }
        }
        else
        {
            /* No event is pending. */
        }
    }
    else
    {
        /* Ignore calls made before successful initialization. */
    }
}

/**
 * \brief Validates the mandatory fields of a static FSM configuration.
 *
 * \return STD_TRUE when valid; otherwise STD_FALSE.
 */
static uint8 Fsm_IsConfigValid(const Fsm_ConfigType * const config)
{
    uint8 ret = STD_FALSE;

    if ((config != NULL_PTR) &&
        (config->stateOperations != NULL_PTR) &&
        (config->transitionTable != NULL_PTR) &&
        (config->stateCount > (Fsm_StateType)0U) &&
        (config->eventCount > (Fsm_EventType)0U) &&
        (config->initialState < config->stateCount) &&
        (config->transitionCount > (uint16)0U))
    {
        if ((Fsm_AreStateOperationsValid(config) == STD_TRUE) &&
            (Fsm_AreTransitionsValid(config) == STD_TRUE))
        {
            ret = STD_TRUE;
        }
        else
        {
            /* At least one state callback or transition row is invalid. */
        }
    }
    else
    {
        ret = STD_FALSE;
    }

    return ret;
}

/** \brief Checks every state-operation callback in an FSM configuration. */
static uint8 Fsm_AreStateOperationsValid(
    const Fsm_ConfigType * const config)
{
    uint8 ret = STD_TRUE;
    uint16 index;

    for (index = (uint16)0U;
         index < (uint16)config->stateCount;
         index++)
    {
        if ((config->stateOperations[index].entry ==
             (Fsm_StateFuncType)0) ||
            (config->stateOperations[index].exit ==
             (Fsm_StateFuncType)0) ||
            (config->stateOperations[index].doAction ==
             (Fsm_StateFuncType)0))
        {
            ret = STD_FALSE;
            break;
        }
        else
        {
            /* Continue validating the remaining state callbacks. */
        }
    }

    return ret;
}

/** \brief Checks state, event, and callback fields in every transition row. */
static uint8 Fsm_AreTransitionsValid(
    const Fsm_ConfigType * const config)
{
    uint8 ret = STD_TRUE;
    uint16 index;

    for (index = (uint16)0U;
         index < config->transitionCount;
         index++)
    {
        const Fsm_TransitionType * const transition =
            &config->transitionTable[index];

        if ((transition->currentState >= config->stateCount) ||
            (transition->nextState >= config->stateCount) ||
            (transition->event >= config->eventCount) ||
            (transition->guard == (Fsm_GuardFuncType)0) ||
            (transition->action == (Fsm_ActionFuncType)0))
        {
            ret = STD_FALSE;
            break;
        }
        else
        {
            /* Continue validating the remaining transition rows. */
        }
    }

    return ret;
}
