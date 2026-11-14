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
    \file   fsm_queue.h
    \brief  Declares the fixed-capacity FIFO queue used by FSM instances.
            The queue uses caller-owned static storage and performs no dynamic
            memory allocation.
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
#ifndef FSM_QUEUE_H
#define FSM_QUEUE_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Maximum number of pending events per FSM instance. */
#define FSM_QUEUE_LENGTH ((uint8)16U)

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Queue element type; compatible with the generic FSM event storage type. */
typedef uint16 FsmQueue_ValueType;

/** Mutable indices and storage for one independent ring queue. */
typedef struct
{
    FsmQueue_ValueType buffer[FSM_QUEUE_LENGTH]; /**< Fixed event storage. */
    uint8 head;  /**< Index of the next value to pop. */
    uint8 tail;  /**< Index at which the next value is pushed. */
    uint8 count; /**< Number of currently stored values. */
} FsmQueue_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** \brief Clears a queue and resets all ring indices. */
void FsmQueue_Init(FsmQueue_ContextType * const context);

/** \brief Pushes one value; returns STD_NOT_OK if null or full. */
Std_ReturnType FsmQueue_Push(FsmQueue_ContextType * const context,
                             FsmQueue_ValueType value);

/** \brief Pops the oldest value; returns STD_NOT_OK if invalid or empty. */
Std_ReturnType FsmQueue_Pop(FsmQueue_ContextType * const context,
                            FsmQueue_ValueType * const value);

/** \brief Returns STD_TRUE when the queue is null or contains no value. */
uint8 FsmQueue_IsEmpty(const FsmQueue_ContextType * const context);

/** \brief Returns STD_TRUE when the queue is null or at capacity. */
uint8 FsmQueue_IsFull(const FsmQueue_ContextType * const context);

#endif /* FSM_QUEUE_H */
