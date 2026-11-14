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
    \file   fsm.h
    \brief  Declares the reusable event-driven, table-based FSM interface.
            Configuration tables may be shared, while state and event queue
            storage remain private to each FSM context.
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
| 2026-11-14 000.03     DL      Added execution guard and reentry diagnostics
| 2026-11-14 000.04     DL      Added compile-time configuration size checks
********************************************************************************/
#ifndef FSM_H
#define FSM_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"
#include "fsm_queue.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Array length without narrowing; argument must be a complete array. */
#define FSM_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/** C99 file-scope assertion: a false condition produces a negative array size. */
#define FSM_CONFIG_STATIC_ASSERT(name, condition) \
    typedef char name[(condition) ? 1 : -1]

/**
 * Declares a checked, static configuration at file scope.
 * Supply complete arrays (never pointers), and uncast STATE_MAX/EVENT_MAX
 * integer constants. State IDs must be contiguous starting at zero.
 * Counts are checked before narrowing and are derived from the same arrays
 * stored in the configuration. End the invocation with a semicolon.
 * Runtime validation still checks initial state, rows, and callbacks.
 */
#define FSM_DEFINE_CONFIG(name, initialStateValue, stateCountValue, \
                          eventCountValue, operationsArray, transitionsArray) \
    FSM_CONFIG_STATIC_ASSERT(name##_StateCountRangeCheck, \
        ((stateCountValue) > 0U) && ((stateCountValue) <= 65535U)); \
    FSM_CONFIG_STATIC_ASSERT(name##_StateTableSizeCheck, \
        FSM_ARRAY_COUNT(operationsArray) == (stateCountValue)); \
    FSM_CONFIG_STATIC_ASSERT(name##_EventCountRangeCheck, \
        ((eventCountValue) > 0U) && ((eventCountValue) <= 65535U)); \
    FSM_CONFIG_STATIC_ASSERT(name##_TransitionCountRangeCheck, \
        (FSM_ARRAY_COUNT(transitionsArray) > 0U) && \
        (FSM_ARRAY_COUNT(transitionsArray) <= 65535U)); \
    static const Fsm_ConfigType name = \
    { \
        (Fsm_StateType)(initialStateValue), \
        (Fsm_StateType)FSM_ARRAY_COUNT(operationsArray), \
        (Fsm_EventType)(eventCountValue), \
        (operationsArray), \
        (transitionsArray), \
        (uint16)FSM_ARRAY_COUNT(transitionsArray) \
    }

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Generic storage types used by all application-specific state/event enums. */
typedef uint16 Fsm_StateType;
typedef uint16 Fsm_EventType;

/**
 * Callback types used by transition tables and state operation tables.
 * Callbacks may post events and query their context, but must not call Init,
 * Dispatch, or Do on the same context or directly modify its internal data.
 */
typedef uint8 (*Fsm_GuardFuncType)(void);
typedef void (*Fsm_ActionFuncType)(void);
typedef void (*Fsm_StateFuncType)(void);

/** Describes one event-triggered transition from one state to another. */
typedef struct
{
    Fsm_StateType currentState; /**< State in which this row is applicable. */
    Fsm_EventType event;        /**< Event that selects this row. */
    Fsm_GuardFuncType guard;    /**< Returns STD_TRUE when transition is allowed. */
    Fsm_ActionFuncType action;  /**< Runs between exit and state update. */
    Fsm_StateType nextState;    /**< State entered after the action. */
} Fsm_TransitionType;

/** Groups the entry, exit, and cyclic do callbacks for one state. */
typedef struct
{
    Fsm_StateFuncType entry;    /**< Called once when the state is entered. */
    Fsm_StateFuncType exit;     /**< Called once before the state is left. */
    Fsm_StateFuncType doAction; /**< Called by Fsm_Do while state is active. */
} Fsm_StateOperationType;

/**
 * Constant configuration supplied by an application FSM module.
 * Use FSM_DEFINE_CONFIG for compile-time array/count checks. Directly built
 * configurations must supply correct lengths; Fsm_Init cannot discover them.
 */
typedef struct
{
    Fsm_StateType initialState; /**< State selected during Fsm_Init. */
    Fsm_StateType stateCount;   /**< Number of valid application states. */
    Fsm_EventType eventCount;   /**< Number of valid application events. */
    const Fsm_StateOperationType * stateOperations; /**< State callback table. */
    const Fsm_TransitionType * transitionTable;     /**< Transition rows. */
    uint16 transitionCount;     /**< Number of transition rows. */
} Fsm_ConfigType;

/**
 * Mutable data belonging to one FSM instance.
 * Zero-initialize the complete context before its first API call. All access
 * must be serialized by the owner; executing is not a task/ISR lock.
 */
typedef struct
{
    Fsm_StateType state;              /**< Current authoritative FSM state. */
    Fsm_EventType lastEvent;          /**< Most recently dequeued event. */
    const Fsm_ConfigType * config;    /**< Read-only configuration reference. */
    FsmQueue_ContextType eventQueue;  /**< Private event queue for this FSM. */
    uint8 initialized;                /**< STD_TRUE after successful init. */
    uint8 executing;                  /**< Owned by Init, Dispatch, or Do. */
    uint16 reentryRejectCount;         /**< Saturating lifetime reject count. */
} Fsm_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Initializes an FSM context and enters its configured initial state.
 *
 * Requires a zero-initialized context before first use. Rejects calls made
 * while Init, Dispatch, or Do is executing on this context. Reinitialization
 * preserves reentry diagnostics; invalid configuration preserves FSM data.
 * \return STD_OK on success; otherwise STD_NOT_OK.
 */
Std_ReturnType Fsm_Init(Fsm_ContextType * const context,
                        const Fsm_ConfigType * const config);

/**
 * \brief Adds one validated event to an FSM instance queue.
 *
 * May be called from callbacks. The caller must handle queue overflow.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType Fsm_PostEvent(Fsm_ContextType * const context,
                             Fsm_EventType event);

/**
 * \brief Removes and processes at most one event from the instance queue.
 *
 * No callback is executed when the queue is empty or the context is invalid.
 * Reentry on the same context is rejected before an event is consumed.
 */
void Fsm_Dispatch(Fsm_ContextType * const context);

/**
 * \brief Executes the cyclic do callback of the current state.
 * Reentry on the same context is rejected without executing a callback.
 */
void Fsm_Do(Fsm_ContextType * const context);

/** \brief Returns the current state, or zero for an invalid context. */
Fsm_StateType Fsm_GetState(const Fsm_ContextType * const context);

/** \brief Returns the last dequeued event, or zero for an invalid context. */
Fsm_EventType Fsm_GetLastEvent(const Fsm_ContextType * const context);

/**
 * \brief Returns the number of rejected Init, Dispatch, and Do reentry calls.
 *
 * Saturates at 65535 and is preserved across Fsm_Init calls. May be queried
 * from callbacks or before initialization on a zero-initialized context.
 * Returns zero for NULL. This function does not clear the count.
 */
uint16 Fsm_GetReentryRejectCount(const Fsm_ContextType * const context);

#endif /* FSM_H */
