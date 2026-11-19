/*******************************************************************************
  4. HEADER FILES
*******************************************************************************/
#include "SWC_NvM.h"
#include "App_NvM_RamBlock.h"
#include "user_defs.h"
#include "user_data.h"

#define HW_VER_SEPARATOR  '.'

uint8 rub_PartNO[10];
uint8 rub_MDate[4];
uint8 rub_SerialNO[10];
uint8 rub_DTCInfo[kDtcInfo_MAX]; /* 8 */
uint8 rub_ACCCntClrDTC[1];
uint8 rub_Noti_Info[kNotiInfo_MAX]; /* 6 */
uint8 rub_SysLogData[52];
uint8 rub_IAU_LearntInfo[50];
uint8 rub_AVN_LearntInfo[50];
uint8 rub_UserInfo[kPosUserInfo_MAX]; /* 8 */
uint8 rub_HWVersionInfo[4];


void Nvm_LoadHWVersion(uint8 *pBuf)
{
  if (pBuf != NULL)
  {
    pBuf[0] = rub_HWVersionInfo[0];
    pBuf[1] = rub_HWVersionInfo[1];
    pBuf[2] = (uint8)HW_VER_SEPARATOR;
    pBuf[3] = rub_HWVersionInfo[2];
    pBuf[4] = rub_HWVersionInfo[3];
  }
}

void Nvm_SaveHWVersion(const uint8 *pBuf)
{
  if (pBuf != NULL)
  {
    rub_HWVersionInfo[0] = pBuf[0];
    rub_HWVersionInfo[1] = pBuf[1];
    /* pBuf[2] is '.' */
    rub_HWVersionInfo[2] = pBuf[3];
    rub_HWVersionInfo[3] = pBuf[4];

    NVM_Set_HWVersionNotiInfo();
  }
}

void Nvm_InitHWVersion(void)
{
  if((rub_HWVersionInfo[0] == 0) && (rub_HWVersionInfo[1] == 0))
  {
    rub_HWVersionInfo[0] = ECU_HWVerINFO_0;
    rub_HWVersionInfo[1] = ECU_HWVerINFO_1;
    rub_HWVersionInfo[2] = ECU_HWVerINFO_2;
    rub_HWVersionInfo[3] = ECU_HWVerINFO_3;
    NVM_Set_HWVersionNotiInfo();
  }
}


/* for PDC reset */
void Nvm_SavePdcSelect(const uint8 vSel)
{
  if(vSel != rub_Noti_Info[kNotiInfo_PDC_ResetOPSte])
  {
    rub_Noti_Info[kNotiInfo_PDC_ResetOPSte] = vSel;
    NVM_Set_PdcNotiInfo();
  }
}

