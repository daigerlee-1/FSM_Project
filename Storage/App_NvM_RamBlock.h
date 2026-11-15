#ifndef APP_NVM_RAM_H
#define APP_NVM_RAM_H

#include "Rte_Type.h"

#include "user_nvm_data.h"

extern uint8 rub_PartNO[10];
extern uint8 rub_MDate[4];
extern uint8 rub_SerialNO[10];
extern uint8 rub_DTCInfo[kDtcInfo_MAX];  /* 8 */
extern uint8 rub_ACCCntClrDTC[1];
extern uint8 rub_Noti_Info[kNotiInfo_MAX]; /* 6 */
extern uint8 rub_SysLogData[52];
extern uint8 rub_IAU_LearntInfo[50];
extern uint8 rub_AVN_LearntInfo[50];
extern uint8 rub_UserInfo[kPosUserInfo_MAX]; /* 8 */
extern uint8 rub_HWVersionInfo[4];


void Nvm_LoadHWVersion(uint8 *pBuf);
void Nvm_SaveHWVersion(const uint8 *pBuf);
void Nvm_InitHWVersion(void);

void Nvm_SavePdcSelect(const uint8 vSel);

#endif /*APP_NVM_RAM_H*/
