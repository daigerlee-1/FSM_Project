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
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Generic storage types used by all application-specific state/event enums. */
typedef uint16 Fsm_StateType;
typedef uint16 Fsm_EventType;

/** Callback types used by transition tables and state operation tables. */
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

/** Constant configuration supplied by an application FSM module. */
typedef struct
{
    Fsm_StateType initialState; /**< State selected during Fsm_Init. */
    Fsm_StateType stateCount;   /**< Number of valid application states. */
    Fsm_EventType eventCount;   /**< Number of valid application events. */
    const Fsm_StateOperationType * stateOperations; /**< State callback table. */
    const Fsm_TransitionType * transitionTable;     /**< Transition rows. */
    uint16 transitionCount;     /**< Number of transition rows. */
} Fsm_ConfigType;

/** Mutable data belonging to one FSM instance. */
typedef struct
{
    Fsm_StateType state;              /**< Current authoritative FSM state. */
    Fsm_EventType lastEvent;          /**< Most recently dequeued event. */
    const Fsm_ConfigType * config;    /**< Read-only configuration reference. */
    FsmQueue_ContextType eventQueue;  /**< Private event queue for this FSM. */
    uint8 initialized;                /**< STD_TRUE after successful init. */
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
 * \return STD_OK on success; otherwise STD_NOT_OK.
 */
Std_ReturnType Fsm_Init(Fsm_ContextType * const context,
                        const Fsm_ConfigType * const config);

/**
 * \brief Adds one validated event to an FSM instance queue.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType Fsm_PostEvent(Fsm_ContextType * const context,
                             Fsm_EventType event);

/**
 * \brief Removes and processes at most one event from the instance queue.
 *
 * No callback is executed when the queue is empty or the context is invalid.
 */
void Fsm_Dispatch(Fsm_ContextType * const context);

/** \brief Executes the cyclic do callback of the current state. */
void Fsm_Do(Fsm_ContextType * const context);

/** \brief Returns the current state, or zero for an invalid context. */
Fsm_StateType Fsm_GetState(const Fsm_ContextType * const context);

/** \brief Returns the last dequeued event, or zero for an invalid context. */
Fsm_EventType Fsm_GetLastEvent(const Fsm_ContextType * const context);

#endif /* FSM_H */
