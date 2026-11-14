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
    \file   fsm_queue.c
    \brief  Implements the fixed-capacity FIFO ring queue for FSM events.
            The count member distinguishes full and empty conditions when the
            head and tail indices are equal.
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
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "fsm_queue.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/* No private definitions or macros are declared by this module. */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private functions are declared by this module. */

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
/** \brief Clears a queue and resets all ring-buffer indices. */
void FsmQueue_Init(FsmQueue_ContextType * const context)
{
    uint8 index;

    if (context != NULL_PTR)
    {
        context->head = (uint8)0U;
        context->tail = (uint8)0U;
        context->count = (uint8)0U;

        /* Clearing storage gives deterministic debugger and trace output. */
        for (index = (uint8)0U; index < FSM_QUEUE_LENGTH; index++)
        {
            context->buffer[index] = (FsmQueue_ValueType)0U;
        }
    }
    else
    {
        /* Invalid context. */
    }
}

/** \brief Pushes one value into a queue when capacity is available. */
Std_ReturnType FsmQueue_Push(FsmQueue_ContextType * const context,
                             FsmQueue_ValueType value)
{
    Std_ReturnType ret = STD_NOT_OK;

    if ((context != NULL_PTR) && (FsmQueue_IsFull(context) == STD_FALSE))
    {
        context->buffer[context->tail] = value;
        context->tail++;

        /* Wrap only after the current write has advanced the tail. */
        if (context->tail >= FSM_QUEUE_LENGTH)
        {
            context->tail = (uint8)0U;
        }
        else
        {
            /* Tail remains within the ring buffer. */
        }

        /* count is incremented only after a value is committed. */
        context->count++;
        ret = STD_OK;
    }
    else
    {
        /* Reject null contexts and full queues. */
    }

    return ret;
}

/** \brief Removes and returns the oldest queued value. */
Std_ReturnType FsmQueue_Pop(FsmQueue_ContextType * const context,
                            FsmQueue_ValueType * const value)
{
    Std_ReturnType ret = STD_NOT_OK;

    if ((context != NULL_PTR) &&
        (value != NULL_PTR) &&
        (FsmQueue_IsEmpty(context) == STD_FALSE))
    {
        *value = context->buffer[context->head];

        /* Clearing the consumed slot is diagnostic, not required by FIFO logic. */
        context->buffer[context->head] = (FsmQueue_ValueType)0U;
        context->head++;

        /* Wrap only after the current read has advanced the head. */
        if (context->head >= FSM_QUEUE_LENGTH)
        {
            context->head = (uint8)0U;
        }
        else
        {
            /* Head remains within the ring buffer. */
        }

        /* count is decremented only after the output value is valid. */
        context->count--;
        ret = STD_OK;
    }
    else
    {
        /* Reject invalid parameters and empty queues. */
    }

    return ret;
}

/** \brief Reports whether a queue is null or empty. */
uint8 FsmQueue_IsEmpty(const FsmQueue_ContextType * const context)
{
    /* A null queue is treated as unavailable and therefore empty. */
    uint8 ret = STD_TRUE;

    if ((context != NULL_PTR) && (context->count > (uint8)0U))
    {
        ret = STD_FALSE;
    }
    else
    {
        ret = STD_TRUE;
    }

    return ret;
}

/** \brief Reports whether a queue is null or full. */
uint8 FsmQueue_IsFull(const FsmQueue_ContextType * const context)
{
    /* A null queue is treated as unavailable and therefore full. */
    uint8 ret = STD_TRUE;

    if ((context != NULL_PTR) && (context->count < FSM_QUEUE_LENGTH))
    {
        ret = STD_FALSE;
    }
    else
    {
        ret = STD_TRUE;
    }

    return ret;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/* No private function implementations are defined by this module. */
