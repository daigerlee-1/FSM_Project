/* RAM-only NvM substitute for serial PC tests; no persistence across Init. */
#include "prj_defs.h"
#if (PRJ_MOBILGENE_ENABLE == 0U)
#include "drv_nvm.h"

static uint8 DrvNvm_Block[DRV_NVM_BLOCK_MAX][DRV_NVM_MAX_BLOCK_LENGTH];
static boolean DrvNvm_BlockValid[DRV_NVM_BLOCK_MAX];
static DrvNvm_RequestResultType DrvNvm_BlockResult[DRV_NVM_BLOCK_MAX];

static boolean DrvNvm_IsValidId(DrvNvm_BlockIdType blockId)
{
    return ((blockId >= DRV_NVM_BLOCK_ACC_CNT) &&
            (blockId < DRV_NVM_BLOCK_MAX)) ? TRUE : FALSE;
}

void DrvNvm_Init(void)
{
    uint16 blockIndex;
    uint16 byteIndex;
    for (blockIndex = 0U; blockIndex < (uint16)DRV_NVM_BLOCK_MAX; blockIndex++)
    {
        for (byteIndex = 0U; byteIndex < DRV_NVM_MAX_BLOCK_LENGTH; byteIndex++)
        {
            DrvNvm_Block[blockIndex][byteIndex] = 0U;
        }
        DrvNvm_BlockValid[blockIndex] = FALSE;
        DrvNvm_BlockResult[blockIndex] = DRV_NVM_REQUEST_FAILED;
    }
}

uint16 DrvNvm_GetBlockLength(DrvNvm_BlockIdType blockId)
{
    uint16 length = 0U;
    switch (blockId)
    {
        case DRV_NVM_BLOCK_ACC_CNT: length = DRV_NVM_ACC_CNT_LENGTH; break;
        case DRV_NVM_BLOCK_AVN_LEARNT_INFO: length = DRV_NVM_AVN_LEARNT_INFO_LENGTH; break;
        case DRV_NVM_BLOCK_DTC_INFO: length = DRV_NVM_DTC_INFO_LENGTH; break;
        case DRV_NVM_BLOCK_HW_VERSION_INFO: length = DRV_NVM_HW_VERSION_INFO_LENGTH; break;
        case DRV_NVM_BLOCK_IAU_LEARNT_INFO: length = DRV_NVM_IAU_LEARNT_INFO_LENGTH; break;
        case DRV_NVM_BLOCK_MDATE: length = DRV_NVM_MDATE_LENGTH; break;
        case DRV_NVM_BLOCK_NOTI_INFO: length = DRV_NVM_NOTI_INFO_LENGTH; break;
        case DRV_NVM_BLOCK_PART_NO: length = DRV_NVM_PART_NO_LENGTH; break;
        case DRV_NVM_BLOCK_SERIAL_NO: length = DRV_NVM_SERIAL_NO_LENGTH; break;
        case DRV_NVM_BLOCK_SYS_LOG_DATA: length = DRV_NVM_SYS_LOG_DATA_LENGTH; break;
        case DRV_NVM_BLOCK_USER_INFO: length = DRV_NVM_USER_INFO_LENGTH; break;
        default: break;
    }
    return length;
}

Std_ReturnType DrvNvm_Read(DrvNvm_BlockIdType blockId, uint8 * const data, uint16 length)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 index;
    if ((DrvNvm_IsValidId(blockId) == TRUE) && (data != NULL_PTR) &&
        (length == DrvNvm_GetBlockLength(blockId)))
    {
        if (DrvNvm_BlockValid[blockId] == TRUE)
        {
            for (index = 0U; index < length; index++)
            {
                data[index] = DrvNvm_Block[blockId][index];
            }
            DrvNvm_BlockResult[blockId] = DRV_NVM_REQUEST_OK;
            result = STD_OK;
        }
        else
        {
            DrvNvm_BlockResult[blockId] = DRV_NVM_REQUEST_FAILED;
        }
    }
    return result;
}

Std_ReturnType DrvNvm_Write(DrvNvm_BlockIdType blockId, const uint8 * const data, uint16 length)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 index;
    if ((DrvNvm_IsValidId(blockId) == TRUE) && (data != NULL_PTR) &&
        (length == DrvNvm_GetBlockLength(blockId)))
    {
        for (index = 0U; index < length; index++)
        {
            DrvNvm_Block[blockId][index] = data[index];
        }
        DrvNvm_BlockValid[blockId] = TRUE;
        DrvNvm_BlockResult[blockId] = DRV_NVM_REQUEST_OK;
        result = STD_OK;
    }
    return result;
}

Std_ReturnType DrvNvm_Erase(DrvNvm_BlockIdType blockId)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 index;
    if (DrvNvm_IsValidId(blockId) == TRUE)
    {
        for (index = 0U; index < DrvNvm_GetBlockLength(blockId); index++)
        {
            DrvNvm_Block[blockId][index] = 0U;
        }
        DrvNvm_BlockValid[blockId] = FALSE;
        DrvNvm_BlockResult[blockId] = DRV_NVM_REQUEST_OK;
        result = STD_OK;
    }
    return result;
}

Std_ReturnType DrvNvm_GetErrorStatus(DrvNvm_BlockIdType blockId,
                                    DrvNvm_RequestResultType * const requestResult)
{
    Std_ReturnType result = STD_NOT_OK;
    if ((DrvNvm_IsValidId(blockId) == TRUE) && (requestResult != NULL_PTR))
    {
        *requestResult = DrvNvm_BlockResult[blockId];
        result = STD_OK;
    }
    return result;
}
#endif /* PRJ_MOBILGENE_ENABLE == 0U */
