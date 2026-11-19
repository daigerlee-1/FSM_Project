#ifndef __SWC_NVM_H__
#define __SWC_NVM_H__

/*============================================================================*/
// INCLUDE(Application)
/*============================================================================*/
/* Include Header File, Application */
#include "user_typedefs.h"
#include "user_defs.h"
#include "user_api.h"


/*******************************************************************************
    10. Function
*******************************************************************************/
void NVM_Set_PdcNotiInfo(void);
uint8 NVM_Get_PdcNotiInfo(void);
void NVM_Set_HWVersionNotiInfo(void);

void NVM_LoadHwVerInfo(uint8 *pBuf);
void NVM_SetHwVerInfo(const uint8 *pBuf);
void NVM_Init_AppOption(void);
uint8 NVM_IsNotWorking(void);

#endif /* __SWC_NVM_H__ */
