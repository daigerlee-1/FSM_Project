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
    \file   lcan_input.h
    \brief  Declares RTE-independent local-CAN input processing interfaces.
            Defines received HU message snapshots, signal-validity states,
            cyclic processing, update services, and timeout notifications.
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
| 2026-10-29 000.01     DL      Creation
| 2026-10-29 000.02     DL      Standardized public header documentation
| 2026-10-30 000.03     DL      Added prioritized LCAN illumination control
********************************************************************************/
#ifndef LCAN_INPUT_H
#define LCAN_INPUT_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Contains the latest L_HU_CarInfo signals and their RTE validity states. */
typedef struct
{
    uint8 avTail;
    uint8 acc;
    uint8 ign1;
    uint8 ign3;
    uint8 rheostatLevel;
    uint8 autoLightSensor;
    uint8 detentOutputState;
    uint8 terminalControlState;
    uint8 driverSeatOccupancy;
    uint8 driverDoorSwitch;
    uint8 faceDetectionState;
    uint8 adm;
    uint8 welcomeStartRequest;
    uint8 driverDoorUnlockState;
    uint8 engineState;
    uint8 hevReadyState;
    uint8 evDriveReadyState;
    boolean avTailValid;
    boolean accValid;
    boolean ign1Valid;
    boolean ign3Valid;
    boolean rheostatLevelValid;
    boolean autoLightSensorValid;
    boolean detentOutputStateValid;
    boolean terminalControlStateValid;
    boolean driverSeatOccupancyValid;
    boolean driverDoorSwitchValid;
    boolean faceDetectionStateValid;
    boolean admValid;
    boolean welcomeStartRequestValid;
    boolean driverDoorUnlockStateValid;
    boolean engineStateValid;
    boolean hevReadyStateValid;
    boolean evDriveReadyStateValid;
} LcanInput_HuCarInfoType;

/** Contains the latest L_HU_E_USM signal and its RTE validity state. */
typedef struct
{
    uint8 moodLampOnOff;
    boolean moodLampOnOffValid;
} LcanInput_HuEUsmType;

/** Contains the latest L_HU_MoodInfo signals and their RTE validity states. */
typedef struct
{
    uint8 slaveColorX;
    uint8 slaveColorY;
    uint8 slaveBrightness;
    uint8 fadeInTime;
    uint8 fadeOutTime;
    uint8 fadeInOutEnabled;
    boolean slaveColorXValid;
    boolean slaveColorYValid;
    boolean slaveBrightnessValid;
    boolean fadeInTimeValid;
    boolean fadeOutTimeValid;
    boolean fadeInOutEnabledValid;
} LcanInput_HuMoodInfoType;

/** Contains the latest L_HU_State signals and their RTE validity states. */
typedef struct
{
    uint8 pwrState;
    uint8 hardResetAvailable;
    uint8 resetPreWarning;
    uint8 resetRequest;
    uint16 autoBrightnessState;
    boolean pwrStateValid;
    boolean hardResetAvailableValid;
    boolean resetPreWarningValid;
    boolean resetRequestValid;
    boolean autoBrightnessStateValid;
} LcanInput_HuStateType;

/** Contains the latest L_HU_SYS signals and their RTE validity states. */
typedef struct
{
    uint8 softwareVersionRequest;
    uint8 hardwareVersionRequest;
    uint8 canVersionRequest;
    boolean softwareVersionRequestValid;
    boolean hardwareVersionRequestValid;
    boolean canVersionRequestValid;
} LcanInput_HuSysType;

/** Contains the latest L_HU_SysInfo signals and their RTE validity states. */
typedef struct
{
    uint8 hapticFeedbackLevel;
    uint8 rotateChangeRequest;
    boolean hapticFeedbackLevelValid;
    boolean rotateChangeRequestValid;
} LcanInput_HuSysInfoType;

/** Tracks delivery of the latest valid HU automatic-brightness request. */
typedef struct
{
    /** Most recent valid HU automatic-brightness step. */
    uint8 requestedStep;

    /** Most recent step accepted by the LED application. */
    uint8 lastAcceptedStep;

    /** Indicates that requestedStep still needs to be delivered. */
    boolean requestPending;

    /** Indicates that lastAcceptedStep contains an accepted value. */
    boolean lastAcceptedStepValid;

    /** Indicates that a valid LCAN command still needs to enable the output. */
    boolean outputEnablePending;
} LcanInput_BrightnessControlType;

/** Collects all local-CAN input message snapshots for runtime observation. */
typedef struct
{
    /** Latest L_HU_CarInfo message snapshot. */
    LcanInput_HuCarInfoType huCarInfo;

    /** Latest L_HU_E_USM message snapshot. */
    LcanInput_HuEUsmType huEUsm;

    /** Latest L_HU_MoodInfo message snapshot. */
    LcanInput_HuMoodInfoType huMoodInfo;

    /** Latest L_HU_State message snapshot. */
    LcanInput_HuStateType huState;

    /** Latest L_HU_SYS message snapshot. */
    LcanInput_HuSysType huSys;

    /** Latest L_HU_SysInfo message snapshot. */
    LcanInput_HuSysInfoType huSysInfo;

    /** HU automatic-brightness delivery state. */
    LcanInput_BrightnessControlType brightnessControl;
} LcanInput_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** Performs cyclic local-CAN input processing at a 10 ms rate. */
void LcanInput_MainFunction10ms(void);

/**
 * \brief Returns the selected vehicle powertrain start/ready condition.
 *
 * ICE evaluates EngSta, HEV evaluates HevRdySta, and EV evaluates
 * EvDrvRdySta. An invalid or timed-out signal always returns FALSE.
 *
 * \return TRUE when the selected valid signal is ON; otherwise FALSE.
 */
boolean LcanInput_IsEngineStarted(void);

/**
 * \brief Returns read-only access to all stored local-CAN input snapshots.
 * \return Pointer to the module-owned input context.
 */
const LcanInput_ContextType * LcanInput_GetContext(void);

/** Stores one L_HU_CarInfo snapshot received through the SWC RTE adapter. */
void LcanInput_UpdateHuCarInfo(const LcanInput_HuCarInfoType *data);

/** Stores one L_HU_E_USM snapshot received through the SWC RTE adapter. */
void LcanInput_UpdateHuEUsm(const LcanInput_HuEUsmType *data);

/** Stores one L_HU_MoodInfo snapshot received through the SWC RTE adapter. */
void LcanInput_UpdateHuMoodInfo(const LcanInput_HuMoodInfoType *data);

/** Stores and distributes one L_HU_State snapshot. */
void LcanInput_UpdateHuState(const LcanInput_HuStateType *data);

/** Stores one L_HU_SYS snapshot received through the SWC RTE adapter. */
void LcanInput_UpdateHuSys(const LcanInput_HuSysType *data);

/** Stores one L_HU_SysInfo snapshot received through the SWC RTE adapter. */
void LcanInput_UpdateHuSysInfo(const LcanInput_HuSysInfoType *data);

/** Invalidates all stored L_HU_CarInfo signals after an RTE timeout. */
void LcanInput_NotifyHuCarInfoTimeout(void);

/** Invalidates all stored L_HU_MoodInfo signals after an RTE timeout. */
void LcanInput_NotifyHuMoodInfoTimeout(void);

/**
 * Invalidates the RTE-level HU power-state snapshot without stopping the
 * state-specific timeout owned by the power application.
 */
void LcanInput_NotifyHuStateTimeout(void);

/** Invalidates all stored L_HU_SysInfo signals after an RTE timeout. */
void LcanInput_NotifyHuSysInfoTimeout(void);

#endif /* LCAN_INPUT_H */
