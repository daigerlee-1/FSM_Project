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
    \file   nvm_app.c
    \brief  Implements the asynchronous NVM application state machine.
            Queued read, write, and erase operations are issued once and
            monitored until NvM reports a terminal request result.
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
| 2026-11-12 000.01     DL      Added asynchronous NvM write management FSM
| 2026-11-12 000.02     DL      Added managed block read and erase requests
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "nvm_app.h"

/* #include "drv_console.h" */
#include "drv_nvm.h"
#include "fsm.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Byte containing the encoded power state in the compatibility record. */
#define NVM_POWER_STATE_INDEX ((uint16)0U)

/** Byte containing the rolling write sequence in the compatibility record. */
#define NVM_SEQUENCE_INDEX ((uint16)1U)

/** Byte containing the XOR checksum in the compatibility record. */
#define NVM_CHECKSUM_INDEX ((uint16)7U)

/**
 * Existing power-state persistence uses the configured 8-byte UserInfo block.
 * Replace this mapping when a dedicated power-state NvM block is configured.
 */
#define NVM_APP_POWER_BLOCK_ID (DRV_NVM_BLOCK_USER_INFO)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are defined. */

/*******************************************************************************
    8. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void NvmApp_EntryIdle(void);
static void NvmApp_EntryReading(void);
static void NvmApp_EntryWriting(void);
static void NvmApp_EntryErasing(void);
static void NvmApp_EntryError(void);
static void NvmApp_ExitNoAction(void);
static void NvmApp_DoIdle(void);
static void NvmApp_DoActiveRequest(void);
static void NvmApp_DoError(void);
static uint8 NvmApp_GuardAlways(void);
static void NvmApp_ActionNone(void);
static uint8 NvmApp_CalculateChecksum(const uint8 * const data);
static void NvmApp_PostJobFailed(void);

/*******************************************************************************
    9. PRIVATE VARIABLES
*******************************************************************************/
/** Runtime context exposed to diagnostics through NvmApp_GetContext(). */
static Nvm_ContextType NvmApp_Context;

/** Retained source buffer used until an asynchronous write is complete. */
static uint8 NvmApp_Shadow[DRV_NVM_MAX_BLOCK_LENGTH];

/** Length of the block currently held in NvmApp_Shadow. */
static uint16 NvmApp_ActiveLength;

/** Rolling sequence stored in the legacy power-state record. */
static uint8 NvmApp_Sequence;

/** Indicates that the driver accepted the active asynchronous operation. */
static boolean NvmApp_OperationAccepted;

/** Prevents another request from replacing the active operation data. */
static boolean NvmApp_RequestQueued;

/** Destination receiving data after a successful asynchronous block read. */
static uint8 *NvmApp_ReadDestination;

/*******************************************************************************
    10. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/** Entry, exit, and cyclic handlers indexed by Nvm_StateType. */
static const Fsm_StateOperationType NvmApp_StateOperations[NVM_STATE_MAX] =
{
    { NvmApp_EntryIdle,    NvmApp_ExitNoAction, NvmApp_DoIdle          },
    { NvmApp_EntryReading, NvmApp_ExitNoAction, NvmApp_DoActiveRequest },
    { NvmApp_EntryWriting, NvmApp_ExitNoAction, NvmApp_DoActiveRequest },
    { NvmApp_EntryErasing, NvmApp_ExitNoAction, NvmApp_DoActiveRequest },
    { NvmApp_EntryError,   NvmApp_ExitNoAction, NvmApp_DoError         }
};

/** Table of all permitted NVM application state transitions. */
static const Fsm_TransitionType NvmApp_Transitions[] =
{
    { (Fsm_StateType)NVM_STATE_IDLE,
      (Fsm_EventType)NVM_EVENT_READ_REQUEST,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_READING },
    { (Fsm_StateType)NVM_STATE_IDLE,
      (Fsm_EventType)NVM_EVENT_WRITE_REQUEST,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_WRITING },
    { (Fsm_StateType)NVM_STATE_IDLE,
      (Fsm_EventType)NVM_EVENT_ERASE_REQUEST,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_ERASING },
    { (Fsm_StateType)NVM_STATE_READING,
      (Fsm_EventType)NVM_EVENT_JOB_DONE,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_IDLE },
    { (Fsm_StateType)NVM_STATE_READING,
      (Fsm_EventType)NVM_EVENT_JOB_FAILED,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_ERROR },
    { (Fsm_StateType)NVM_STATE_WRITING,
      (Fsm_EventType)NVM_EVENT_JOB_DONE,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_IDLE },
    { (Fsm_StateType)NVM_STATE_WRITING,
      (Fsm_EventType)NVM_EVENT_JOB_FAILED,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_ERROR },
    { (Fsm_StateType)NVM_STATE_ERASING,
      (Fsm_EventType)NVM_EVENT_JOB_DONE,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_IDLE },
    { (Fsm_StateType)NVM_STATE_ERASING,
      (Fsm_EventType)NVM_EVENT_JOB_FAILED,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_ERROR },
    { (Fsm_StateType)NVM_STATE_ERROR,
      (Fsm_EventType)NVM_EVENT_RESET,
      NvmApp_GuardAlways, NvmApp_ActionNone,
      (Fsm_StateType)NVM_STATE_IDLE }
};

#define NVM_TRANSITION_COUNT                                      \
    ((uint16)(sizeof(NvmApp_Transitions) /                        \
              sizeof(NvmApp_Transitions[0])))

/** Generic FSM configuration for the NVM application. */
static const Fsm_ConfigType NvmApp_Config =
{
    (Fsm_StateType)NVM_STATE_IDLE,
    (Fsm_StateType)NVM_STATE_MAX,
    (Fsm_EventType)NVM_EVENT_MAX,
    NvmApp_StateOperations,
    NvmApp_Transitions,
    NVM_TRANSITION_COUNT
};

/*******************************************************************************
    11. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes the driver, retained buffer, context, and FSM. */
void NvmApp_Init(void)
{
    uint16 index;

    DrvNvm_Init();
    NvmApp_ActiveLength = (uint16)0U;
    NvmApp_Sequence = (uint8)0U;
    NvmApp_OperationAccepted = FALSE;
    NvmApp_RequestQueued = FALSE;
    NvmApp_ReadDestination = NULL_PTR;

    for (index = (uint16)0U; index < DRV_NVM_MAX_BLOCK_LENGTH; index++)
    {
        NvmApp_Shadow[index] = (uint8)0U;
    }

    (void)Fsm_Init(&NvmApp_Context.fsm, &NvmApp_Config);
    NvmApp_Context.state = NVM_STATE_IDLE;
    NvmApp_Context.event = NVM_EVENT_MAX;
    NvmApp_Context.activeBlock = DRV_NVM_BLOCK_MAX;
    NvmApp_Context.requestResult = DRV_NVM_REQUEST_FAILED;
}

/**
 * \brief Dispatches one event, runs state behavior, and updates the context.
 */
void NvmApp_MainFunction10ms(void)
{
    Fsm_Dispatch(&NvmApp_Context.fsm);
    Fsm_Do(&NvmApp_Context.fsm);
    NvmApp_Context.state =
        (Nvm_StateType)Fsm_GetState(&NvmApp_Context.fsm);
    NvmApp_Context.event =
        (Nvm_EventType)Fsm_GetLastEvent(&NvmApp_Context.fsm);
}

/** \brief Builds and queues the legacy 8-byte power-state record. */
Std_ReturnType NvmApp_RequestWrite(uint8 powerState)
{
    Std_ReturnType result = STD_NOT_OK;
    uint8 powerData[DRV_NVM_USER_INFO_LENGTH];
    uint16 index;

    if ((NvmApp_Context.state == NVM_STATE_IDLE) &&
        (NvmApp_RequestQueued == FALSE))
    {
        for (index = (uint16)0U;
             index < DRV_NVM_USER_INFO_LENGTH;
             index++)
        {
            powerData[index] = (uint8)0U;
        }

        NvmApp_Sequence++;
        powerData[NVM_POWER_STATE_INDEX] = powerState;
        powerData[NVM_SEQUENCE_INDEX] = NvmApp_Sequence;
        powerData[NVM_CHECKSUM_INDEX] =
            NvmApp_CalculateChecksum(powerData);

        result = NvmApp_RequestBlockWrite(
            NVM_APP_POWER_BLOCK_ID,
            powerData,
            DRV_NVM_USER_INFO_LENGTH);
    }
    else
    {
        /* Reject a second request while an NvM write is outstanding. */
    }

    return result;
}

/** \brief Queues one generic block read request. */
Std_ReturnType NvmApp_RequestBlockRead(
    DrvNvm_BlockIdType blockId,
    uint8 * const data,
    uint16 length)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((NvmApp_Context.state == NVM_STATE_IDLE) &&
        (NvmApp_RequestQueued == FALSE) &&
        (data != NULL_PTR) &&
        (length == DrvNvm_GetBlockLength(blockId)) &&
        (length <= DRV_NVM_MAX_BLOCK_LENGTH))
    {
        NvmApp_Context.activeBlock = blockId;
        NvmApp_Context.requestResult = DRV_NVM_REQUEST_PENDING;
        NvmApp_ActiveLength = length;
        NvmApp_ReadDestination = data;
        result = Fsm_PostEvent(&NvmApp_Context.fsm,
                               (Fsm_EventType)NVM_EVENT_READ_REQUEST);
        if (result == STD_OK)
        {
            NvmApp_RequestQueued = TRUE;
        }
        else
        {
            NvmApp_Context.requestResult = DRV_NVM_REQUEST_FAILED;
            NvmApp_Context.activeBlock = DRV_NVM_BLOCK_MAX;
            NvmApp_ActiveLength = (uint16)0U;
            NvmApp_ReadDestination = NULL_PTR;
        }
    }

    return result;
}

/** \brief Copies and queues one generic block write request. */
Std_ReturnType NvmApp_RequestBlockWrite(
    DrvNvm_BlockIdType blockId,
    const uint8 * const data,
    uint16 length)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 index;

    if ((NvmApp_Context.state == NVM_STATE_IDLE) &&
        (NvmApp_RequestQueued == FALSE) &&
        (data != NULL_PTR) &&
        (length == DrvNvm_GetBlockLength(blockId)) &&
        (length <= DRV_NVM_MAX_BLOCK_LENGTH))
    {
        for (index = (uint16)0U; index < length; index++)
        {
            NvmApp_Shadow[index] = data[index];
        }

        NvmApp_Context.activeBlock = blockId;
        NvmApp_Context.requestResult = DRV_NVM_REQUEST_PENDING;
        NvmApp_ActiveLength = length;
        NvmApp_ReadDestination = NULL_PTR;
        result = Fsm_PostEvent(&NvmApp_Context.fsm,
                               (Fsm_EventType)NVM_EVENT_WRITE_REQUEST);
        if (result == STD_OK)
        {
            NvmApp_RequestQueued = TRUE;
        }
        else
        {
            NvmApp_Context.requestResult = DRV_NVM_REQUEST_FAILED;
            NvmApp_Context.activeBlock = DRV_NVM_BLOCK_MAX;
            NvmApp_ActiveLength = (uint16)0U;
        }
    }

    return result;
}

/** \brief Queues one generic block erase request. */
Std_ReturnType NvmApp_RequestBlockErase(DrvNvm_BlockIdType blockId)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((NvmApp_Context.state == NVM_STATE_IDLE) &&
        (NvmApp_RequestQueued == FALSE) &&
        ((uint16)blockId < (uint16)DRV_NVM_BLOCK_MAX))
    {
        NvmApp_Context.activeBlock = blockId;
        NvmApp_Context.requestResult = DRV_NVM_REQUEST_PENDING;
        NvmApp_ActiveLength = (uint16)0U;
        NvmApp_ReadDestination = NULL_PTR;
        result = Fsm_PostEvent(&NvmApp_Context.fsm,
                               (Fsm_EventType)NVM_EVENT_ERASE_REQUEST);
        if (result == STD_OK)
        {
            NvmApp_RequestQueued = TRUE;
        }
        else
        {
            NvmApp_Context.requestResult = DRV_NVM_REQUEST_FAILED;
            NvmApp_Context.activeBlock = DRV_NVM_BLOCK_MAX;
        }
    }

    return result;
}

/** \brief Queues recovery from the error state. */
Std_ReturnType NvmApp_RequestReset(void)
{
    return Fsm_PostEvent(&NvmApp_Context.fsm,
                         (Fsm_EventType)NVM_EVENT_RESET);
}

/**
 * \brief Returns the power-state byte currently stored in the shadow buffer.
 */
uint8 NvmApp_GetStoredPowerState(void)
{
    return NvmApp_Shadow[NVM_POWER_STATE_INDEX];
}

/** \brief Returns a read-only view of the NVM application context. */
const Nvm_ContextType * NvmApp_GetContext(void)
{
    return &NvmApp_Context;
}

/*******************************************************************************
    12. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Clears request-control flags when the FSM enters IDLE. */
static void NvmApp_EntryIdle(void)
{
    NvmApp_ActiveLength = (uint16)0U;
    NvmApp_OperationAccepted = FALSE;
    NvmApp_RequestQueued = FALSE;
    NvmApp_ReadDestination = NULL_PTR;
    /* DrvConsole_WriteString("[NVM] state IDLE\r\n"); */
}

/** \brief Issues the retained read request exactly once on state entry. */
static void NvmApp_EntryReading(void)
{
    if (DrvNvm_Read(NvmApp_Context.activeBlock,
                    NvmApp_Shadow,
                    NvmApp_ActiveLength) != STD_OK)
    {
        NvmApp_PostJobFailed();
    }
    else
    {
        NvmApp_OperationAccepted = TRUE;
    }

    /* DrvConsole_WriteString("[NVM] state READING\r\n"); */
}

/** \brief Issues the retained write request exactly once on state entry. */
static void NvmApp_EntryWriting(void)
{
    if (DrvNvm_Write(NvmApp_Context.activeBlock,
                     NvmApp_Shadow,
                     NvmApp_ActiveLength) != STD_OK)
    {
        NvmApp_PostJobFailed();
    }
    else
    {
        NvmApp_OperationAccepted = TRUE;
    }

    /* DrvConsole_WriteString("[NVM] state WRITING\r\n"); */
}

/** \brief Issues the selected erase request exactly once on state entry. */
static void NvmApp_EntryErasing(void)
{
    if (DrvNvm_Erase(NvmApp_Context.activeBlock) != STD_OK)
    {
        NvmApp_PostJobFailed();
    }
    else
    {
        NvmApp_OperationAccepted = TRUE;
    }

    /* DrvConsole_WriteString("[NVM] state ERASING\r\n"); */
}

/** \brief Reports entry into the terminal error state. */
static void NvmApp_EntryError(void)
{
    NvmApp_OperationAccepted = FALSE;
    NvmApp_ReadDestination = NULL_PTR;
    /* DrvConsole_WriteString("[NVM] state ERROR\r\n"); */
}

/** \brief Provides a common no-operation exit handler. */
static void NvmApp_ExitNoAction(void)
{
    /* No exit processing is required. */
}

/** \brief Waits for the next accepted NVM operation request. */
static void NvmApp_DoIdle(void)
{
    /* Wait for a read, write, or erase request. */
}

/** \brief Polls NvM status and emits the corresponding completion event. */
static void NvmApp_DoActiveRequest(void)
{
    DrvNvm_RequestResultType requestResult = DRV_NVM_REQUEST_FAILED;
    uint16 index;

    if (NvmApp_OperationAccepted == FALSE)
    {
        /* State entry already queued JOB_FAILED after request rejection. */
    }
    else if (DrvNvm_GetErrorStatus(NvmApp_Context.activeBlock,
                                   &requestResult) == STD_OK)
    {
        NvmApp_Context.requestResult = requestResult;

        switch (requestResult)
        {
            case DRV_NVM_REQUEST_OK:
                if (((Nvm_StateType)Fsm_GetState(&NvmApp_Context.fsm) ==
                     NVM_STATE_READING) &&
                    (NvmApp_ReadDestination != NULL_PTR))
                {
                    for (index = (uint16)0U;
                         index < NvmApp_ActiveLength;
                         index++)
                    {
                        NvmApp_ReadDestination[index] = NvmApp_Shadow[index];
                    }
                }
                else
                {
                    /* Write and erase operations require no result copy. */
                }

                (void)Fsm_PostEvent(&NvmApp_Context.fsm,
                                    (Fsm_EventType)NVM_EVENT_JOB_DONE);
                break;
            case DRV_NVM_REQUEST_PENDING:
                /* Keep the active state until NvM reports a terminal result. */
                break;
            case DRV_NVM_REQUEST_FAILED:
            default:
                NvmApp_PostJobFailed();
                break;
        }
    }
    else
    {
        NvmApp_PostJobFailed();
    }
}

/** \brief Waits for an explicit reset request while in the error state. */
static void NvmApp_DoError(void)
{
    /* Wait for an explicit reset request. */
}

/** \brief Unconditionally permits a configured FSM transition. */
static uint8 NvmApp_GuardAlways(void)
{
    return STD_TRUE;
}

/** \brief Provides a common no-operation transition action. */
static void NvmApp_ActionNone(void)
{
    /* Entry and do functions perform the NVM behavior. */
}

/** \brief Calculates the XOR checksum of the legacy power-state record. */
static uint8 NvmApp_CalculateChecksum(const uint8 * const data)
{
    uint8 checksum = (uint8)0U;
    uint16 index;

    for (index = (uint16)0U; index < NVM_CHECKSUM_INDEX; index++)
    {
        checksum ^= data[index];
    }

    return checksum;
}

/** \brief Records request failure and queues the common FSM failure event. */
static void NvmApp_PostJobFailed(void)
{
    NvmApp_OperationAccepted = FALSE;
    NvmApp_Context.requestResult = DRV_NVM_REQUEST_FAILED;
    (void)Fsm_PostEvent(&NvmApp_Context.fsm,
                        (Fsm_EventType)NVM_EVENT_JOB_FAILED);
}
