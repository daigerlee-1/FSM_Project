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
    \file   nvm_app.h
    \brief  Declares the asynchronous NVM application state machine.
            The module serializes write requests, retains write buffers, and
            monitors NvM completion before returning to the idle state.
********************************************************************************
|-------------------------------------------------------------------------------
|    3. A U T H O R   I D E N T I T Y
|-------------------------------------------------------------------------------
| Initials     Name                      Company
| --------     ---------------------     ---------------------------------------
| DL           lee juhwan(DaigerLee)     Mobaseelectronic software design team
|
|-------------------------------------------------------------------------------
|               R E V I S I O N   H I S T O R Y
|-------------------------------------------------------------------------------
| Date       Ver        Author  Description
| ---------  ---        ------  ------------------------------------------------
| 2026-11-12 000.01     DL      Added asynchronous block-write FSM
| 2026-11-12 000.02     DL      Added managed block read and erase requests
********************************************************************************/
#ifndef NVM_APP_H
#define NVM_APP_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "drv_nvm.h"
#include "fsm.h"

/*******************************************************************************
    5. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** NVM application FSM states. */
typedef enum
{
    NVM_STATE_IDLE = 0U,
    NVM_STATE_READING,
    NVM_STATE_WRITING,
    NVM_STATE_ERASING,
    NVM_STATE_ERROR,
    NVM_STATE_MAX
} Nvm_StateType;

/** Events accepted by the NVM application FSM. */
typedef enum
{
    NVM_EVENT_READ_REQUEST = 0U,
    NVM_EVENT_WRITE_REQUEST,
    NVM_EVENT_ERASE_REQUEST,
    NVM_EVENT_JOB_DONE,
    NVM_EVENT_JOB_FAILED,
    NVM_EVENT_RESET,
    NVM_EVENT_MAX
} Nvm_EventType;

/** Runtime context exposed as a read-only diagnostic view. */
typedef struct
{
    /** Current externally visible application state. */
    Nvm_StateType state;

    /** Most recently dispatched FSM event. */
    Nvm_EventType event;

    /** NvM block associated with the current asynchronous request. */
    DrvNvm_BlockIdType activeBlock;

    /** Latest normalized request status reported by the driver layer. */
    DrvNvm_RequestResultType requestResult;

    /** Generic table-driven FSM context. */
    Fsm_ContextType fsm;
} Nvm_ContextType;

/*******************************************************************************
    6. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** \brief Initializes the NVM application context and state machine. */
void NvmApp_Init(void);

/** \brief Processes one event and executes state behavior every 10 ms. */
void NvmApp_MainFunction10ms(void);

/**
 * \brief Requests persistence of the legacy power-state record.
 * \param[in] powerState Power state encoded in the compatibility record.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType NvmApp_RequestWrite(uint8 powerState);

/**
 * \brief Queues one block read managed by the NVM application FSM.
 * \param[in] blockId Logical NvM block identifier.
 * \param[out] data Destination that remains valid until the request completes.
 * \param[in] length Number of bytes; it must match the configured block.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType NvmApp_RequestBlockRead(
    DrvNvm_BlockIdType blockId,
    uint8 * const data,
    uint16 length);

/**
 * \brief Queues one block write and copies data into retained storage.
 * \param[in] blockId Logical NvM block identifier.
 * \param[in] data Source data copied before this function returns.
 * \param[in] length Number of bytes; it must match the configured block.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType NvmApp_RequestBlockWrite(
    DrvNvm_BlockIdType blockId,
    const uint8 * const data,
    uint16 length);

/**
 * \brief Queues erasure of one block through the NVM application FSM.
 * \param[in] blockId Logical NvM block identifier.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType NvmApp_RequestBlockErase(DrvNvm_BlockIdType blockId);

/** \brief Requests recovery from NVM_STATE_ERROR. */
Std_ReturnType NvmApp_RequestReset(void);

/** \brief Returns the power-state byte held in the current shadow buffer. */
uint8 NvmApp_GetStoredPowerState(void);

/** \brief Returns the read-only NVM application diagnostic context. */
const Nvm_ContextType * NvmApp_GetContext(void);

#endif /* NVM_APP_H */
