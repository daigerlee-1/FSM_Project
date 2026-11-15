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
    \file   SWC_NVM_MGR.c
    \brief  Implements the AUTOSAR RTE adapter for persistent-data management.
            This module provides the cyclic NVM manager runnable and receives
            NvM job-finished notifications for project-specific data blocks.
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
| 2026-11-11 000.01     DL      Creation
| 2026-11-12 000.02     DL      Added typed storage for configured NvM blocks
| 2026-11-12 000.03     DL      Connected asynchronous NVM application FSM
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_NVM_MGR.h"
#include "NvM_Ram.h" 

#include "nvm_app.h"

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

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Indicates whether the application-side NVM manager has been initialized. */
static boolean SwcNvmMgr_Initialized = FALSE;

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private functions are declared by this module. */
#define OLD_NVM_M

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_NVM_MGR_START_SEC_CODE
#include "SWC_NVM_MGR_MemMap.h"



#ifdef OLD_NVM_M
#include "NvM.h"
#include "App_NvM_RamBlock.h"
#include "SWC_NvM.h"

/* Include Header File, Application */
#include "user_typedefs.h"
#include "user_defs.h"
#include "user_api.h"


/*============================================================================*/
// Declaration of Global Variables
/*============================================================================*/
#define MAX_NVM_RETRY       100

/* Check Correspond of NvM Status */
Std_ReturnType NvmRetval = E_OK;
NvM_RequestResultType BlockStatus = NVM_REQ_OK;
/* Declaration Used NvMemory Working State Check */
u16 b_WorkNvMStatus = d_NvMState_NotWork;
u8 NvM_save_Chk = kNVM_SaveSte_Default;
/* Declaration Used Diagnostic WDBI */
u8 Req_WRNvM_PartNO = Off;
u8 Req_WRNvM_SerialNO = Off;
u8 Req_WRNvM_MDate = Off;
/* Declaration Used DTC Process Application Function */
u8 Req_WRNvM_DTCInfo = Off;
u8 Req_WRNvM_ACCCnt = Off;
/* Declaration Used FPM Notification & System Log Data */
u8 Req_WRNvM_NotiInfo = Off;
u8 Req_WRNvM_NotiInfo_RetryCnt = 0u;
u8 Req_WRNvM_SysLogData = Off;
/* Declaration Used FPM Notification & System Log Data */
u8 Req_WRNvM_IAULearntInfo = Off;
u8 Req_WRNvM_AVNLearntInfo = Off;
/* Declaration Used User Information Data */
u8 Req_WRNvM_UserInfo = Off;
/* Declaration Used FPM Functions */
u8 save_swap_area = Off;
u8 save_fpm_eeprom_USEROPTION = Off;
u8 save_fpm_eeprom_EOL_Programming_Start = Off;
u8 save_fpm_eeprom_EOL_Programmed_Check = Off;
/* Declaration HW Version Information Data */
u8 Req_WRNvM_HWVersionInfo = Off;

#endif

/**
 * \brief Executes periodic NVM manager processing every 10 ms.
 *
 * The runnable is reserved for asynchronous request handling and status
 * monitoring. No cyclic operation is required by the current implementation.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NVM_MGR_10ms_Task(void)
{
    if (SwcNvmMgr_Initialized == FALSE)
    {
        NvmApp_Init();
        SwcNvmMgr_Initialized = TRUE;
    }
    else
    {
        /* The application-side manager is already initialized. */
    }

    if ((SwcNvmMgr_Initialized == TRUE) &&
        (NvM_GblInitDone == TRUE))
    {
        NvmApp_MainFunction10ms();
    }
    else
    {
        /* Keep queued requests pending until BSW NvM startup is complete. */
    }


    /* No periodic NVM manager processing is currently required. */
#ifdef OLD_NVM_M
    static uint8 b_NvmInit = On;

    if(b_NvmInit == On)
    {
        b_NvmInit = Off;

        if(NvM_GblInitDone == 1 /* (boolean)NVM_TRUE */) 
        {
            NVM_Init_AppOption();
        }
        else
        {
            b_NvmInit = On;
        }
    }
    else
    {
        /* OEM Part Number Write Process When a Requset PartNO Flag Set */
        if(Req_WRNvM_PartNO==On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_PartNO_GetErrorStatus(&BlockStatus);
            if((NvmRetval==E_OK)&&(BlockStatus!=NVM_REQ_NOT_OK))
            {
                if(BlockStatus!=NVM_REQ_PENDING)
                {
                    b_WorkNvMStatus |= d_WkBitPos_PartNO;
                    (void)Rte_Call_PS_NvMBlock_PartNO_WriteBlock(rub_PartNO);
                    NvM_save_Chk = kNVM_SaveSte_Success;
                }
            }
            else {
                NvM_save_Chk = kNVM_SaveSte_Fail;
            }
            Req_WRNvM_PartNO = Off;
        }
        /* Manufacture Date Write Process When a Requset MDate Flag Set */
        if(Req_WRNvM_MDate==On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_MDate_GetErrorStatus(&BlockStatus);
            if((NvmRetval==E_OK)&&(BlockStatus!=NVM_REQ_NOT_OK))
            {
                if(BlockStatus!=NVM_REQ_PENDING)
                {
                    b_WorkNvMStatus |= d_WkBitPos_MDate;
                    (void)Rte_Call_PS_NvMBlock_MDate_WriteBlock(rub_MDate);
                    NvM_save_Chk = kNVM_SaveSte_Success;
                }
            }
            else {
                NvM_save_Chk = kNVM_SaveSte_Fail;
            }
            Req_WRNvM_MDate = Off;
        }
        /* Serial Number Write Process When a Requset SerialNO Flag Set */
        if(Req_WRNvM_SerialNO==On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_SerialNO_GetErrorStatus(&BlockStatus);
            if((NvmRetval==E_OK)&&(BlockStatus!=NVM_REQ_NOT_OK))
            {
                if(BlockStatus!=NVM_REQ_PENDING)
                {
                    b_WorkNvMStatus |= d_WkBitPos_SerialNO;
                    (void)Rte_Call_PS_NvMBlock_SerialNO_WriteBlock(rub_SerialNO);
                    NvM_save_Chk = kNVM_SaveSte_Success;
                }
            }
            Req_WRNvM_SerialNO = Off;
        }

        /* HW Version Write Process When a Request HWVersionInfo Flag Set */
        if(Req_WRNvM_HWVersionInfo==On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_HWVersionInfo_GetErrorStatus(&BlockStatus);
            if((NvmRetval==E_OK)&&(BlockStatus!=NVM_REQ_NOT_OK))
            {
                if(BlockStatus!=NVM_REQ_PENDING)
                {
                    b_WorkNvMStatus |= d_WkBitPos_HWVersionInfo;
                    (void)Rte_Call_PS_NvMBlock_HWVersionInfo_WriteBlock(rub_HWVersionInfo);
                    NvM_save_Chk = kNVM_SaveSte_Success;
                }
            }
            Req_WRNvM_HWVersionInfo = Off;
        }

        /* for PDC reset */
        if(Req_WRNvM_NotiInfo == On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_Noti_Info_GetErrorStatus(&BlockStatus);
            if((NvmRetval == (Std_ReturnType)E_OK) && (BlockStatus != NVM_REQ_PENDING))
            {
                NvmRetval = Rte_Call_PS_NvMBlock_Noti_Info_WriteBlock(rub_Noti_Info);
                if(NvmRetval == (Std_ReturnType)E_OK)
                {
                    b_WorkNvMStatus |= d_WkBitPos_NotiInfo;
                    Req_WRNvM_NotiInfo = Off; 
                    NvM_save_Chk = kNVM_SaveSte_Success;
                    Req_WRNvM_NotiInfo_RetryCnt = 0u;
                }
                else
                {
                    Req_WRNvM_NotiInfo_RetryCnt++;
                    if(Req_WRNvM_NotiInfo_RetryCnt > MAX_NVM_RETRY)
                    {
                        Req_WRNvM_NotiInfo = Off; 
                        NvM_save_Chk = kNVM_SaveSte_Fail;
                    }
                }
            }
        }

#if 0
        /* DTC Information Status Write Process When a Requset DTCInfo Flag Set */
        if(Req_WRNvM_DTCInfo==On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_DTCInfo_GetErrorStatus(&BlockStatus);
            if((NvmRetval==E_OK)&&(BlockStatus!=NVM_REQ_NOT_OK))
            {
                if(BlockStatus!=NVM_REQ_PENDING)
                {
                    b_WorkNvMStatus |= d_WkBitPos_DTCInfo;
                    (void)Rte_Call_PS_NvMBlock_DTCInfo_WriteBlock(rub_DTCInfo);
                    NvM_save_Chk = kNVM_SaveSte_Success;
                }
            }
            else {
                NvM_save_Chk = kNVM_SaveSte_Fail;
            }
            Req_WRNvM_DTCInfo = Off;
        }
#endif
        /* ACC Count Value Write Process When a Requset ACC Count Flag Set */
        if(Req_WRNvM_ACCCnt==On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_ACCCnt_GetErrorStatus(&BlockStatus);
            if((NvmRetval==E_OK)&&(BlockStatus!=NVM_REQ_NOT_OK))
            {
                if(BlockStatus!=NVM_REQ_PENDING)
                {
                    b_WorkNvMStatus |= d_WkBitPos_ACCCnt4DTC;
                    (void)Rte_Call_PS_NvMBlock_ACCCnt_WriteBlock(rub_ACCCntClrDTC);
                    NvM_save_Chk = kNVM_SaveSte_Success;
                }
            }
            else {
                NvM_save_Chk = kNVM_SaveSte_Fail;
            }
            Req_WRNvM_ACCCnt = Off;
        }

        /* User Information Write Process When a Requset UserInfo Flag Set */
        if(Req_WRNvM_UserInfo==On)
        {
            NvmRetval = Rte_Call_PS_NvMBlock_UserInfo_GetErrorStatus(&BlockStatus);
            if((NvmRetval==E_OK)&&(BlockStatus!=NVM_REQ_NOT_OK))
            {
                if(BlockStatus!=NVM_REQ_PENDING)
                {
                    b_WorkNvMStatus |= d_WkBitPos_UserInfo;
                    (void)Rte_Call_PS_NvMBlock_UserInfo_WriteBlock(rub_UserInfo);
                    NvM_save_Chk = kNVM_SaveSte_Success;
                }
            }
            else {
                NvM_save_Chk = kNVM_SaveSte_Fail;
            }
            Req_WRNvM_UserInfo = Off;
        }
    }
#else
#endif
}

/**
 * \brief Receives completion of the ACC counter NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_ACCCnt(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
	if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_ACCCnt4DTC);
	}
#else
#endif

}

/**
 * \brief Receives completion of the AVN learned-information NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_AVN_LearntInfo(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
	if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_AVNLearntInfo);
	}
#else
#endif

}

/**
 * \brief Receives completion of the DTC-information NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_DTCInfo(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_DTCInfo);
	}

#else
#endif

}

/**
 * \brief Receives completion of the hardware-version NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_HWVersionInfo(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
    if(ServiceId==NVM_WRITEBLOCK_SID) {
        b_WorkNvMStatus &= (u8)(~d_WkBitPos_HWVersionInfo);
    }
#else
#endif

}

/**
 * \brief Receives completion of the IAU learned-information NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_IAU_LearntInfo(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M

	if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_IAULearntInfo);
	}
#else
#endif

}

/**
 * \brief Receives completion of the manufacturing-date NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_MDate(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
    if(ServiceId==NVM_WRITEBLOCK_SID) {
        b_WorkNvMStatus &= (u8)(~d_WkBitPos_MDate);
    }
#else
#endif

}

/**
 * \brief Receives completion of the notification-information NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_Noti_Info(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
	if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_NotiInfo);
	}

#else
#endif

}

/**
 * \brief Receives completion of the part-number NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_PartNO(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_PartNO);
	}

#else
#endif
}

/**
 * \brief Receives completion of the serial-number NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_SerialNO(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_SerialNO);
	}

#else
#endif
}

/**
 * \brief Receives completion of the system-log NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_SysLogData(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_SysLogData);
	}

#else
#endif
}

/**
 * \brief Receives completion of the user-information NvM block job.
 * \param[in] ServiceId NvM service identifier that completed.
 * \param[in] JobResult Result reported by NvM for the completed job.
 */
FUNC(void, SWC_NVM_MGR_CODE) RE_NvMJobFinished_UserInfo(
    IN VAR(uint8, AUTOMATIC) ServiceId,
    IN VAR(NvM_RequestResultType, AUTOMATIC) JobResult)
{
#ifdef OLD_NVM_M
if(ServiceId==NVM_WRITEBLOCK_SID) {
		b_WorkNvMStatus &= (u8)(~d_WkBitPos_UserInfo);
	}

#else
#endif
}

#ifdef OLD_NVM_M
void NVM_Init_AppOption(void)
{
  Nvm_InitHWVersion();
} 

uint8 NVM_IsNotWorking(void)
{
  uint8 rVal = Off;

  if(b_WorkNvMStatus == d_NvMState_NotWork)
  {
    rVal = On;
  }

  return rVal;
} 

void NVM_Set_HWVersionNotiInfo(void)
{
  Req_WRNvM_HWVersionInfo = On;
}

void NVM_LoadHwVerInfo(uint8 *pBuf)
{
  Nvm_LoadHWVersion(pBuf);
}

void NVM_SetHwVerInfo(const uint8 *pBuf)
{
  Nvm_SaveHWVersion(pBuf);
}

void NVM_Set_PdcNotiInfo(void)
{
  Req_WRNvM_NotiInfo = On;
}

uint8 NVM_Get_PdcNotiInfo(void)
{
  return Req_WRNvM_NotiInfo;
}
#endif

#define SWC_NVM_MGR_STOP_SEC_CODE
#include "SWC_NVM_MGR_MemMap.h"

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/* No private functions are implemented by this module. */
