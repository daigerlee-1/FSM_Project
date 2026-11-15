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
    \file   lcan_input_rte.c
    \brief  Implements the AUTOSAR RTE adapter for local-CAN input messages.
            Generated RTE values are converted into validated snapshots and
            forwarded to the RTE-independent LCAN input application module.
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
| 2026-11-12 000.01     DL      Separated LCAN input RTE signal acquisition
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_LCAN.h"

#include "lcan_input.h"
#include "lcan_input_rte.h"

/*******************************************************************************
    5. PRIVATE VARIABLES
*******************************************************************************/
/** Most recently read L_HU_CarInfo values and their validity states. */
static LcanInput_HuCarInfoType LcanInputRte_HuCarInfoRx;

/** Most recently read L_HU_E_USM value and its validity state. */
static LcanInput_HuEUsmType LcanInputRte_HuEUsmRx;

/** Most recently read L_HU_MoodInfo values and their validity states. */
static LcanInput_HuMoodInfoType LcanInputRte_HuMoodInfoRx;

/** Most recently read L_HU_State values and their validity states. */
static LcanInput_HuStateType LcanInputRte_HuStateRx;

/** Most recently read L_HU_SYS values and their validity states. */
static LcanInput_HuSysType LcanInputRte_HuSysRx;

/** Most recently read L_HU_SysInfo values and their validity states. */
static LcanInput_HuSysInfoType LcanInputRte_HuSysInfoRx;

/*******************************************************************************
    6. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void LcanInputRte_ReadHuCarInfoPowerSignals(void);
static void LcanInputRte_ReadHuCarInfoLightingSignals(void);
static void LcanInputRte_ReadHuCarInfoDriverSignals(void);
static void LcanInputRte_InvalidateHuCarInfo(void);

/*******************************************************************************
    7. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_LCAN_START_SEC_CODE
#include "SWC_LCAN_MemMap.h"

/** \brief Reads and forwards one complete L_HU_CarInfo snapshot. */
void LcanInputRte_ReadHuCarInfo(void)
{
    LcanInputRte_ReadHuCarInfoPowerSignals();
    LcanInputRte_ReadHuCarInfoLightingSignals();
    LcanInputRte_ReadHuCarInfoDriverSignals();
    LcanInput_UpdateHuCarInfo(&LcanInputRte_HuCarInfoRx);
}

/** \brief Invalidates L_HU_CarInfo after an RTE reception timeout. */
void LcanInputRte_NotifyHuCarInfoTimeout(void)
{
    LcanInputRte_InvalidateHuCarInfo();
    LcanInput_NotifyHuCarInfoTimeout();
}

/** \brief Reads and forwards one L_HU_E_USM snapshot. */
void LcanInputRte_ReadHuEUsm(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_E_USM_CF_AVN_CCP_MoodLampOnOff(
        &LcanInputRte_HuEUsmRx.moodLampOnOff);
    LcanInputRte_HuEUsmRx.moodLampOnOffValid =
        (rteResult == E_OK) ? TRUE : FALSE;
    LcanInput_UpdateHuEUsm(&LcanInputRte_HuEUsmRx);
}

/** \brief Reads and forwards one L_HU_MoodInfo snapshot. */
void LcanInputRte_ReadHuMoodInfo(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_MoodInfo_HU_L_SlvColor_X(
        &LcanInputRte_HuMoodInfoRx.slaveColorX);
    LcanInputRte_HuMoodInfoRx.slaveColorXValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_MoodInfo_HU_L_SlvColor_Y(
        &LcanInputRte_HuMoodInfoRx.slaveColorY);
    LcanInputRte_HuMoodInfoRx.slaveColorYValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_MoodInfo_HU_L_SlvBrgtnsVal(
        &LcanInputRte_HuMoodInfoRx.slaveBrightness);
    LcanInputRte_HuMoodInfoRx.slaveBrightnessValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_MoodInfo_HU_L_SlvFadeInTimetVal(
        &LcanInputRte_HuMoodInfoRx.fadeInTime);
    LcanInputRte_HuMoodInfoRx.fadeInTimeValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_MoodInfo_HU_L_SlvFadeOutTimetVal(
        &LcanInputRte_HuMoodInfoRx.fadeOutTime);
    LcanInputRte_HuMoodInfoRx.fadeOutTimeValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_MoodInfo_HU_L_SlvFadeInOutOnOff(
        &LcanInputRte_HuMoodInfoRx.fadeInOutEnabled);
    LcanInputRte_HuMoodInfoRx.fadeInOutEnabledValid =
        (rteResult == E_OK) ? TRUE : FALSE;
    LcanInput_UpdateHuMoodInfo(&LcanInputRte_HuMoodInfoRx);
}

/** \brief Invalidates L_HU_MoodInfo after an RTE reception timeout. */
void LcanInputRte_NotifyHuMoodInfoTimeout(void)
{
    LcanInputRte_HuMoodInfoRx.slaveColorXValid = FALSE;
    LcanInputRte_HuMoodInfoRx.slaveColorYValid = FALSE;
    LcanInputRte_HuMoodInfoRx.slaveBrightnessValid = FALSE;
    LcanInputRte_HuMoodInfoRx.fadeInTimeValid = FALSE;
    LcanInputRte_HuMoodInfoRx.fadeOutTimeValid = FALSE;
    LcanInputRte_HuMoodInfoRx.fadeInOutEnabledValid = FALSE;
    LcanInput_NotifyHuMoodInfoTimeout();
}

/** \brief Reads and forwards one L_HU_SYS snapshot. */
void LcanInputRte_ReadHuSys(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_SYS_SYS_SW_Ver_Req(
        &LcanInputRte_HuSysRx.softwareVersionRequest);
    LcanInputRte_HuSysRx.softwareVersionRequestValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_SYS_SYS_CAN_Ver_Req(
        &LcanInputRte_HuSysRx.canVersionRequest);
    LcanInputRte_HuSysRx.canVersionRequestValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_SYS_SYS_HW_Ver_Req(
        &LcanInputRte_HuSysRx.hardwareVersionRequest);
    LcanInputRte_HuSysRx.hardwareVersionRequestValid =
        (rteResult == E_OK) ? TRUE : FALSE;
    LcanInput_UpdateHuSys(&LcanInputRte_HuSysRx);
}

/** \brief Reads and forwards one L_HU_State snapshot. */
void LcanInputRte_ReadHuState(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_State_HU_PwrState(
        &LcanInputRte_HuStateRx.pwrState);
    LcanInputRte_HuStateRx.pwrStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_State_HU_HardReset_Available(
        &LcanInputRte_HuStateRx.hardResetAvailable);
    LcanInputRte_HuStateRx.hardResetAvailableValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_State_HU_ResetPreWrngForOthers(
        &LcanInputRte_HuStateRx.resetPreWarning);
    LcanInputRte_HuStateRx.resetPreWarningValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_State_HU_ResetReqForOthers(
        &LcanInputRte_HuStateRx.resetRequest);
    LcanInputRte_HuStateRx.resetRequestValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_State_HU_L_AutoBrightState_New(
        &LcanInputRte_HuStateRx.autoBrightnessState);
    LcanInputRte_HuStateRx.autoBrightnessStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;
    LcanInput_UpdateHuState(&LcanInputRte_HuStateRx);
}

/** \brief Invalidates L_HU_State after an RTE reception timeout. */
void LcanInputRte_NotifyHuStateTimeout(void)
{
    LcanInputRte_HuStateRx.pwrStateValid = FALSE;
    LcanInputRte_HuStateRx.hardResetAvailableValid = FALSE;
    LcanInputRte_HuStateRx.resetPreWarningValid = FALSE;
    LcanInputRte_HuStateRx.resetRequestValid = FALSE;
    LcanInputRte_HuStateRx.autoBrightnessStateValid = FALSE;
    LcanInput_NotifyHuStateTimeout();
}

/** \brief Reads and forwards one L_HU_SysInfo snapshot. */
void LcanInputRte_ReadHuSysInfo(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_SysInfo_CCP_HptFdbckLvl(
        &LcanInputRte_HuSysInfoRx.hapticFeedbackLevel);
    LcanInputRte_HuSysInfoRx.hapticFeedbackLevelValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_SysInfo_CCP_RotateChangeReq(
        &LcanInputRte_HuSysInfoRx.rotateChangeRequest);
    LcanInputRte_HuSysInfoRx.rotateChangeRequestValid =
        (rteResult == E_OK) ? TRUE : FALSE;
    LcanInput_UpdateHuSysInfo(&LcanInputRte_HuSysInfoRx);
}

/** \brief Invalidates L_HU_SysInfo after an RTE reception timeout. */
void LcanInputRte_NotifyHuSysInfoTimeout(void)
{
    LcanInputRte_HuSysInfoRx.hapticFeedbackLevelValid = FALSE;
    LcanInputRte_HuSysInfoRx.rotateChangeRequestValid = FALSE;
    LcanInput_NotifyHuSysInfoTimeout();
}

/*******************************************************************************
    8. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Reads powertrain-related L_HU_CarInfo signals. */
static void LcanInputRte_ReadHuCarInfoPowerSignals(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_AV_Tail(
        &LcanInputRte_HuCarInfoRx.avTail);
    LcanInputRte_HuCarInfoRx.avTailValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_ACC(
        &LcanInputRte_HuCarInfoRx.acc);
    LcanInputRte_HuCarInfoRx.accValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_IGN1(
        &LcanInputRte_HuCarInfoRx.ign1);
    LcanInputRte_HuCarInfoRx.ign1Valid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_IGN3(
        &LcanInputRte_HuCarInfoRx.ign3);
    LcanInputRte_HuCarInfoRx.ign3Valid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_TrmnlCtrlGrpStaBDCEV(
        &LcanInputRte_HuCarInfoRx.terminalControlState);
    LcanInputRte_HuCarInfoRx.terminalControlStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_L_ENG_EngSta(
        &LcanInputRte_HuCarInfoRx.engineState);
    LcanInputRte_HuCarInfoRx.engineStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_L_HCU_HevRdySta(
        &LcanInputRte_HuCarInfoRx.hevReadyState);
    LcanInputRte_HuCarInfoRx.hevReadyStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_L_VCU_EvDrvRdySta(
        &LcanInputRte_HuCarInfoRx.evDriveReadyState);
    LcanInputRte_HuCarInfoRx.evDriveReadyStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;
}

/** \brief Reads lighting-related L_HU_CarInfo signals. */
static void LcanInputRte_ReadHuCarInfoLightingSignals(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_Clu_RheostatLvl(
        &LcanInputRte_HuCarInfoRx.rheostatLevel);
    LcanInputRte_HuCarInfoRx.rheostatLevelValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_AutoLightSensor(
        &LcanInputRte_HuCarInfoRx.autoLightSensor);
    LcanInputRte_HuCarInfoRx.autoLightSensorValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_CLU_DtntOutSta_L(
        &LcanInputRte_HuCarInfoRx.detentOutputState);
    LcanInputRte_HuCarInfoRx.detentOutputStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_ADM(
        &LcanInputRte_HuCarInfoRx.adm);
    LcanInputRte_HuCarInfoRx.admValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_L_CLU_WelcomeStartReq(
        &LcanInputRte_HuCarInfoRx.welcomeStartRequest);
    LcanInputRte_HuCarInfoRx.welcomeStartRequestValid =
        (rteResult == E_OK) ? TRUE : FALSE;
}

/** \brief Reads driver-related L_HU_CarInfo signals. */
static void LcanInputRte_ReadHuCarInfoDriverSignals(void)
{
    Std_ReturnType rteResult;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_DrvStOccSta(
        &LcanInputRte_HuCarInfoRx.driverSeatOccupancy);
    LcanInputRte_HuCarInfoRx.driverSeatOccupancyValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_DrvDrSwSta(
        &LcanInputRte_HuCarInfoRx.driverDoorSwitch);
    LcanInputRte_HuCarInfoRx.driverDoorSwitchValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_C_ICC_FaceDetectStat(
        &LcanInputRte_HuCarInfoRx.faceDetectionState);
    LcanInputRte_HuCarInfoRx.faceDetectionStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;

    rteResult = Rte_Read_LCAN_L_HU_CarInfo_L_DoorLock_DrvDrUnLkSta(
        &LcanInputRte_HuCarInfoRx.driverDoorUnlockState);
    LcanInputRte_HuCarInfoRx.driverDoorUnlockStateValid =
        (rteResult == E_OK) ? TRUE : FALSE;
}

/** \brief Invalidates every L_HU_CarInfo signal in the RTE snapshot. */
static void LcanInputRte_InvalidateHuCarInfo(void)
{
    LcanInputRte_HuCarInfoRx.avTailValid = FALSE;
    LcanInputRte_HuCarInfoRx.accValid = FALSE;
    LcanInputRte_HuCarInfoRx.ign1Valid = FALSE;
    LcanInputRte_HuCarInfoRx.ign3Valid = FALSE;
    LcanInputRte_HuCarInfoRx.rheostatLevelValid = FALSE;
    LcanInputRte_HuCarInfoRx.autoLightSensorValid = FALSE;
    LcanInputRte_HuCarInfoRx.detentOutputStateValid = FALSE;
    LcanInputRte_HuCarInfoRx.terminalControlStateValid = FALSE;
    LcanInputRte_HuCarInfoRx.driverSeatOccupancyValid = FALSE;
    LcanInputRte_HuCarInfoRx.driverDoorSwitchValid = FALSE;
    LcanInputRte_HuCarInfoRx.faceDetectionStateValid = FALSE;
    LcanInputRte_HuCarInfoRx.admValid = FALSE;
    LcanInputRte_HuCarInfoRx.welcomeStartRequestValid = FALSE;
    LcanInputRte_HuCarInfoRx.driverDoorUnlockStateValid = FALSE;
    LcanInputRte_HuCarInfoRx.engineStateValid = FALSE;
    LcanInputRte_HuCarInfoRx.hevReadyStateValid = FALSE;
    LcanInputRte_HuCarInfoRx.evDriveReadyStateValid = FALSE;
}

#define SWC_LCAN_STOP_SEC_CODE
#include "SWC_LCAN_MemMap.h"
