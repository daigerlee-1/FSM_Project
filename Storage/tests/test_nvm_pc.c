#include <assert.h>
#include "nvm_app.h"

int main(void)
{
    uint8 input[DRV_NVM_MAX_BLOCK_LENGTH];
    uint8 output[DRV_NVM_MAX_BLOCK_LENGTH];
    DrvNvm_BlockIdType blockId;
    DrvNvm_RequestResultType status;
    uint16 index;
    uint16 length;
    DrvNvm_Init();
    for (blockId = DRV_NVM_BLOCK_ACC_CNT; blockId < DRV_NVM_BLOCK_MAX; blockId++)
    {
        length = DrvNvm_GetBlockLength(blockId);
        for (index = 0U; index < length; index++)
        {
            input[index] = (uint8)(index + (uint16)blockId);
            output[index] = 0xAAU;
        }
        assert(DrvNvm_Read(blockId, output, length) == STD_NOT_OK);
        assert(output[0] == 0xAAU);
        assert(DrvNvm_Write(blockId, input, length) == STD_OK);
        assert(DrvNvm_GetErrorStatus(blockId, &status) == STD_OK);
        assert(status == DRV_NVM_REQUEST_OK);
        assert(DrvNvm_Read(blockId, output, length) == STD_OK);
        for (index = 0U; index < length; index++)
        {
            assert(output[index] == input[index]);
        }
        assert(DrvNvm_Erase(blockId) == STD_OK);
        assert(DrvNvm_Read(blockId, output, length) == STD_NOT_OK);
    }
    assert(DrvNvm_Write(DRV_NVM_BLOCK_MAX, input, 0U) == STD_NOT_OK);
    assert(DrvNvm_Write((DrvNvm_BlockIdType)-1, input, 0U) == STD_NOT_OK);
    assert(DrvNvm_Write(DRV_NVM_BLOCK_ACC_CNT, NULL_PTR, 1U) == STD_NOT_OK);
    assert(DrvNvm_Write(DRV_NVM_BLOCK_ACC_CNT, input, 0U) == STD_NOT_OK);
    assert(DrvNvm_GetErrorStatus(DRV_NVM_BLOCK_ACC_CNT, NULL_PTR) == STD_NOT_OK);

    NvmApp_Init();
    input[0] = 42U;
    assert(NvmApp_RequestBlockWrite(DRV_NVM_BLOCK_ACC_CNT, input, 1U) == STD_OK);
    input[0] = 99U;
    NvmApp_MainFunction10ms();
    NvmApp_MainFunction10ms();
    assert(NvmApp_GetContext()->state == NVM_STATE_IDLE);
    assert(NvmApp_GetContext()->requestResult == DRV_NVM_REQUEST_OK);
    output[0] = 0U;
    assert(NvmApp_RequestBlockRead(DRV_NVM_BLOCK_ACC_CNT, output, 1U) == STD_OK);
    NvmApp_MainFunction10ms();
    NvmApp_MainFunction10ms();
    assert(output[0] == 42U);
    return 0;
}
