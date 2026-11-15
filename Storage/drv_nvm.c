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
    \file   drv_nvm.c
    \brief  Implements the project NvM service abstraction.
            AUTOSAR builds dispatch operations to generated RTE NvM ports,
            while standalone builds use deterministic RAM-backed storage.
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
| 2026-11-12 000.01     DL      Added RTE and standalone NvM block adapters
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_NVM_MGR.h"

#include "drv_nvm.h"

#include "prj_defs.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/* No private macros are defined by this module. */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are defined. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/** Validates a block identifier, data pointer, and exact buffer length. */
static boolean DrvNvm_IsRequestValid(
    DrvNvm_BlockIdType blockId,
    const void * const data,
    uint16 length);

/** Converts the AUTOSAR NvM result into the driver-independent result type. */
static DrvNvm_RequestResultType DrvNvm_ConvertRequestResult(
    NvM_RequestResultType requestResult);

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/**
 * \brief Initializes standalone NvM state; target NvM is initialized by BSW.
 */
void DrvNvm_Init(void)
{
#if (PRJ_MOBILGENE_ENABLE == 0U)
    uint16 blockIndex;
    uint16 byteIndex;

    for (blockIndex = (uint16)0U;
         blockIndex < (uint16)DRV_NVM_BLOCK_MAX;
         blockIndex++)
    {
        for (byteIndex = (uint16)0U;
             byteIndex < DRV_NVM_MAX_BLOCK_LENGTH;
             byteIndex++)
        {
            DrvNvm_Block[blockIndex][byteIndex] = (uint8)0U;
        }

        DrvNvm_BlockValid[blockIndex] = FALSE;
        DrvNvm_BlockResult[blockIndex] = DRV_NVM_REQUEST_FAILED;
    }
#else
    /* AUTOSAR NvM and its configured RAM blocks are initialized by BSW. */
#endif
}

/** \brief Returns the configured size for the selected logical NvM block. */
uint16 DrvNvm_GetBlockLength(DrvNvm_BlockIdType blockId)
{
    uint16 length;

    switch (blockId)
    {
        case DRV_NVM_BLOCK_ACC_CNT:
            length = DRV_NVM_ACC_CNT_LENGTH;
            break;
        case DRV_NVM_BLOCK_AVN_LEARNT_INFO:
            length = DRV_NVM_AVN_LEARNT_INFO_LENGTH;
            break;
        case DRV_NVM_BLOCK_DTC_INFO:
            length = DRV_NVM_DTC_INFO_LENGTH;
            break;
        case DRV_NVM_BLOCK_HW_VERSION_INFO:
            length = DRV_NVM_HW_VERSION_INFO_LENGTH;
            break;
        case DRV_NVM_BLOCK_IAU_LEARNT_INFO:
            length = DRV_NVM_IAU_LEARNT_INFO_LENGTH;
            break;
        case DRV_NVM_BLOCK_MDATE:
            length = DRV_NVM_MDATE_LENGTH;
            break;
        case DRV_NVM_BLOCK_NOTI_INFO:
            length = DRV_NVM_NOTI_INFO_LENGTH;
            break;
        case DRV_NVM_BLOCK_PART_NO:
            length = DRV_NVM_PART_NO_LENGTH;
            break;
        case DRV_NVM_BLOCK_SERIAL_NO:
            length = DRV_NVM_SERIAL_NO_LENGTH;
            break;
        case DRV_NVM_BLOCK_SYS_LOG_DATA:
            length = DRV_NVM_SYS_LOG_DATA_LENGTH;
            break;
        case DRV_NVM_BLOCK_USER_INFO:
            length = DRV_NVM_USER_INFO_LENGTH;
            break;
        case DRV_NVM_BLOCK_MAX:
        default:
            length = (uint16)0U;
            break;
    }

    return length;
}

/** \brief Starts a block read through RTE or the standalone RAM substitute. */
Std_ReturnType DrvNvm_Read(
    DrvNvm_BlockIdType blockId,
    uint8 * const data,
    uint16 length)
{
    Std_ReturnType result = STD_NOT_OK;

    if (DrvNvm_IsRequestValid(blockId, data, length) == TRUE)
    {
        switch (blockId)
        {
            case DRV_NVM_BLOCK_ACC_CNT:
                result = Rte_Call_PS_NvMBlock_ACCCnt_ReadBlock((VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_AVN_LEARNT_INFO:
                result =
                    Rte_Call_PS_NvMBlock_AVN_LearntInfo_ReadBlock(
                        (VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_DTC_INFO:
                result = Rte_Call_PS_NvMBlock_DTCInfo_ReadBlock((VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_HW_VERSION_INFO:
                result =
                    Rte_Call_PS_NvMBlock_HWVersionInfo_ReadBlock(
                        (VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_IAU_LEARNT_INFO:
                result =
                    Rte_Call_PS_NvMBlock_IAU_LearntInfo_ReadBlock(
                        (VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_MDATE:
                result = Rte_Call_PS_NvMBlock_MDate_ReadBlock((VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_NOTI_INFO:
                result = Rte_Call_PS_NvMBlock_Noti_Info_ReadBlock(
                    (VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_PART_NO:
                result = Rte_Call_PS_NvMBlock_PartNO_ReadBlock((VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_SERIAL_NO:
                result = Rte_Call_PS_NvMBlock_SerialNO_ReadBlock((VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_SYS_LOG_DATA:
                result = Rte_Call_PS_NvMBlock_SysLogData_ReadBlock(
                    (VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_USER_INFO:
                result = Rte_Call_PS_NvMBlock_UserInfo_ReadBlock((VoidPtr)data);
                break;
            case DRV_NVM_BLOCK_MAX:
            default:
                result = STD_NOT_OK;
                break;
        }
    }

    return result;
}

/** \brief Starts a block write through RTE or the standalone RAM substitute. */
Std_ReturnType DrvNvm_Write(
    DrvNvm_BlockIdType blockId,
    const uint8 * const data,
    uint16 length)
{
    Std_ReturnType result = STD_NOT_OK;

    if (DrvNvm_IsRequestValid(blockId, data, length) == TRUE)
    {
        switch (blockId)
        {
            case DRV_NVM_BLOCK_ACC_CNT:
                result = Rte_Call_PS_NvMBlock_ACCCnt_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_AVN_LEARNT_INFO:
                result =
                    Rte_Call_PS_NvMBlock_AVN_LearntInfo_WriteBlock(
                        (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_DTC_INFO:
                result = Rte_Call_PS_NvMBlock_DTCInfo_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_HW_VERSION_INFO:
                result =
                    Rte_Call_PS_NvMBlock_HWVersionInfo_WriteBlock(
                        (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_IAU_LEARNT_INFO:
                result =
                    Rte_Call_PS_NvMBlock_IAU_LearntInfo_WriteBlock(
                        (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_MDATE:
                result = Rte_Call_PS_NvMBlock_MDate_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_NOTI_INFO:
                result = Rte_Call_PS_NvMBlock_Noti_Info_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_PART_NO:
                result = Rte_Call_PS_NvMBlock_PartNO_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_SERIAL_NO:
                result = Rte_Call_PS_NvMBlock_SerialNO_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_SYS_LOG_DATA:
                result = Rte_Call_PS_NvMBlock_SysLogData_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_USER_INFO:
                result = Rte_Call_PS_NvMBlock_UserInfo_WriteBlock(
                    (ConstVoidPtr)data);
                break;
            case DRV_NVM_BLOCK_MAX:
            default:
                result = STD_NOT_OK;
                break;
        }
    }

    return result;
}

/** \brief Starts erasure of the selected NvM block. */
Std_ReturnType DrvNvm_Erase(DrvNvm_BlockIdType blockId)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((uint16)blockId < (uint16)DRV_NVM_BLOCK_MAX)
    {
        switch (blockId)
        {
            case DRV_NVM_BLOCK_ACC_CNT:
                result = Rte_Call_PS_NvMBlock_ACCCnt_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_AVN_LEARNT_INFO:
                result = Rte_Call_PS_NvMBlock_AVN_LearntInfo_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_DTC_INFO:
                result = Rte_Call_PS_NvMBlock_DTCInfo_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_HW_VERSION_INFO:
                result = Rte_Call_PS_NvMBlock_HWVersionInfo_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_IAU_LEARNT_INFO:
                result = Rte_Call_PS_NvMBlock_IAU_LearntInfo_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_MDATE:
                result = Rte_Call_PS_NvMBlock_MDate_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_NOTI_INFO:
                result = Rte_Call_PS_NvMBlock_Noti_Info_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_PART_NO:
                result = Rte_Call_PS_NvMBlock_PartNO_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_SERIAL_NO:
                result = Rte_Call_PS_NvMBlock_SerialNO_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_SYS_LOG_DATA:
                result = Rte_Call_PS_NvMBlock_SysLogData_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_USER_INFO:
                result = Rte_Call_PS_NvMBlock_UserInfo_EraseNvBlock();
                break;
            case DRV_NVM_BLOCK_MAX:
            default:
                result = STD_NOT_OK;
                break;
        }
    }

    return result;
}

/** \brief Reads and normalizes the asynchronous status of a block request. */
Std_ReturnType DrvNvm_GetErrorStatus(
    DrvNvm_BlockIdType blockId,
    DrvNvm_RequestResultType * const requestResult)
{
    Std_ReturnType result = STD_NOT_OK;

    if (((uint16)blockId < (uint16)DRV_NVM_BLOCK_MAX) &&
        (requestResult != NULL_PTR))
    {
        NvM_RequestResultType rteRequestResult = NVM_REQ_NOT_OK;

        switch (blockId)
        {
            case DRV_NVM_BLOCK_ACC_CNT:
                result = Rte_Call_PS_NvMBlock_ACCCnt_GetErrorStatus(
                    &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_AVN_LEARNT_INFO:
                result =
                    Rte_Call_PS_NvMBlock_AVN_LearntInfo_GetErrorStatus(
                        &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_DTC_INFO:
                result = Rte_Call_PS_NvMBlock_DTCInfo_GetErrorStatus(
                    &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_HW_VERSION_INFO:
                result =
                    Rte_Call_PS_NvMBlock_HWVersionInfo_GetErrorStatus(
                        &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_IAU_LEARNT_INFO:
                result =
                    Rte_Call_PS_NvMBlock_IAU_LearntInfo_GetErrorStatus(
                        &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_MDATE:
                result = Rte_Call_PS_NvMBlock_MDate_GetErrorStatus(
                    &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_NOTI_INFO:
                result = Rte_Call_PS_NvMBlock_Noti_Info_GetErrorStatus(
                    &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_PART_NO:
                result = Rte_Call_PS_NvMBlock_PartNO_GetErrorStatus(
                    &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_SERIAL_NO:
                result = Rte_Call_PS_NvMBlock_SerialNO_GetErrorStatus(
                    &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_SYS_LOG_DATA:
                result =
                    Rte_Call_PS_NvMBlock_SysLogData_GetErrorStatus(
                        &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_USER_INFO:
                result = Rte_Call_PS_NvMBlock_UserInfo_GetErrorStatus(
                    &rteRequestResult);
                break;
            case DRV_NVM_BLOCK_MAX:
            default:
                result = STD_NOT_OK;
                break;
        }

        if (result == E_OK)
        {
            *requestResult = DrvNvm_ConvertRequestResult(rteRequestResult);
        }
    }

    return result;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Checks the common preconditions for block read and write requests. */
static boolean DrvNvm_IsRequestValid(
    DrvNvm_BlockIdType blockId,
    const void * const data,
    uint16 length)
{
    boolean valid = FALSE;

    if (((uint16)blockId < (uint16)DRV_NVM_BLOCK_MAX) &&
        (data != NULL_PTR) &&
        (length == DrvNvm_GetBlockLength(blockId)))
    {
        valid = TRUE;
    }

    return valid;
}

/** \brief Maps AUTOSAR NvM request results to application-visible results. */
static DrvNvm_RequestResultType DrvNvm_ConvertRequestResult(
    NvM_RequestResultType requestResult)
{
    DrvNvm_RequestResultType result;

    switch (requestResult)
    {
        case NVM_REQ_OK:
            result = DRV_NVM_REQUEST_OK;
            break;
        case NVM_REQ_PENDING:
            result = DRV_NVM_REQUEST_PENDING;
            break;
        case NVM_REQ_NOT_OK:
        default:
            result = DRV_NVM_REQUEST_FAILED;
            break;
    }

    return result;
}
