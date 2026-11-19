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
    \file   lcan_input.c
    \brief  Implements RTE-independent local-CAN input data processing.
            Received HU message snapshots are retained with per-signal validity
            and relevant power and LED requests are forwarded to applications.
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
| 2026-10-29 000.02     DL      Standardized C source documentation
| 2026-10-30 000.03     DL      Added prioritized LCAN illumination control
| 2026-10-30 000.04     DL      Enabled output after valid brightness update
| 2026-11-11 000.05     DL      Invalidated all HU State data on timeout
| 2026-11-12 000.06     DL      Added vehicle-specific powertrain condition
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "lcan_input.h"
#include "prj_defs.h"

#include "knob_led.h"
#include "knob_led_cfg.h"
#include "SWC_PWR_MGR.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** CLU detent-output value that overrides automatic brightness. */
#define LCAN_INPUT_DETENT_OUTPUT_ON ((uint8)1U)

/** Brightness step used when HU automatic brightness is zero. */
#define LCAN_INPUT_AUTOBRIGHT_ZERO_STEP ((uint8)13U)

/** Active value of the selected engine or drive-ready CAN signal. */
#define LCAN_INPUT_POWERTRAIN_READY ((uint8)1U)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private types or structures are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void LcanInput_UpdateBrightnessRequest(void);
static void LcanInput_ProcessBrightnessRequest(void);
static boolean LcanInput_GetRequestedBrightness(uint8 * const brightnessStep);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** All local-CAN input snapshots retained for processing and observation. */
static LcanInput_ContextType LcanInput_Context;

/*******************************************************************************
    9. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Performs cyclic local-CAN input processing at a 10 ms rate. */
void LcanInput_MainFunction10ms(void)
{
    LcanInput_ProcessBrightnessRequest();
}

/** \brief Returns the configured vehicle powertrain start/ready condition. */
boolean LcanInput_IsEngineStarted(void)
{
    boolean engineStarted = FALSE;

#if (VEHICLE_POWERTRAIN_TYPE == VEHICLE_ICE)
    if ((LcanInput_Context.huCarInfo.engineStateValid == TRUE) &&
        (LcanInput_Context.huCarInfo.engineState ==
         LCAN_INPUT_POWERTRAIN_READY))
    {
        engineStarted = TRUE;
    }
    else
    {
        /* The ICE engine-state signal is invalid or not started. */
    }
#elif (VEHICLE_POWERTRAIN_TYPE == VEHICLE_HEV)
    if ((LcanInput_Context.huCarInfo.hevReadyStateValid == TRUE) &&
        (LcanInput_Context.huCarInfo.hevReadyState ==
         LCAN_INPUT_POWERTRAIN_READY))
    {
        engineStarted = TRUE;
    }
    else
    {
        /* The HEV ready-state signal is invalid or not ready. */
    }
#elif (VEHICLE_POWERTRAIN_TYPE == VEHICLE_EV)
    if ((LcanInput_Context.huCarInfo.evDriveReadyStateValid == TRUE) &&
        (LcanInput_Context.huCarInfo.evDriveReadyState ==
         LCAN_INPUT_POWERTRAIN_READY))
    {
        engineStarted = TRUE;
    }
    else
    {
        /* The EV drive-ready signal is invalid or not ready. */
    }
#else
    /* The project header rejects unsupported powertrain selections. */
#endif

    return engineStarted;
}

/** \brief Returns read-only access to the module-owned input context. */
const LcanInput_ContextType * LcanInput_GetContext(void)
{
    return &LcanInput_Context;
}

/** \brief Stores one L_HU_CarInfo snapshot and reports CAN reception. */
void LcanInput_UpdateHuCarInfo(const LcanInput_HuCarInfoType *data)
{
    if (data != NULL_PTR)
    {
        LcanInput_Context.huCarInfo = *data;
        LcanInput_UpdateBrightnessRequest();
        (void)SWC_PWR_MGR_NotifyCanRx();
    }
    else
    {
        /* Ignore an invalid integration pointer. */
    }
}

/** \brief Stores one L_HU_E_USM snapshot and reports CAN reception. */
void LcanInput_UpdateHuEUsm(const LcanInput_HuEUsmType *data)
{
    if (data != NULL_PTR)
    {
        LcanInput_Context.huEUsm = *data;
        (void)SWC_PWR_MGR_NotifyCanRx();
    }
    else
    {
        /* Ignore an invalid integration pointer. */
    }
}

/** \brief Stores one L_HU_MoodInfo snapshot and reports CAN reception. */
void LcanInput_UpdateHuMoodInfo(const LcanInput_HuMoodInfoType *data)
{
    if (data != NULL_PTR)
    {
        LcanInput_Context.huMoodInfo = *data;
        (void)SWC_PWR_MGR_NotifyCanRx();
    }
    else
    {
        /* Ignore an invalid integration pointer. */
    }
}

/** \brief Stores and distributes one L_HU_State snapshot. */
void LcanInput_UpdateHuState(const LcanInput_HuStateType *data)
{
    if (data != NULL_PTR)
    {
        LcanInput_Context.huState = *data;
        (void)SWC_PWR_MGR_NotifyCanRx();
        if (data->pwrStateValid == TRUE)
        {
            (void)SWC_PWR_MGR_NotifyHuPowerState(data->pwrState);
        }
        else
        {
            /* Do not forward an invalid HU power-state value. */
        }

        LcanInput_UpdateBrightnessRequest();
    }
    else
    {
        /* Ignore an invalid integration pointer. */
    }
}

/** \brief Stores one L_HU_SYS snapshot and reports CAN reception. */
void LcanInput_UpdateHuSys(const LcanInput_HuSysType *data)
{
    if (data != NULL_PTR)
    {
        LcanInput_Context.huSys = *data;
        (void)SWC_PWR_MGR_NotifyCanRx();
    }
    else
    {
        /* Ignore an invalid integration pointer. */
    }
}

/** \brief Stores one L_HU_SysInfo snapshot and reports CAN reception. */
void LcanInput_UpdateHuSysInfo(const LcanInput_HuSysInfoType *data)
{
    if (data != NULL_PTR)
    {
        LcanInput_Context.huSysInfo = *data;
        (void)SWC_PWR_MGR_NotifyCanRx();
    }
    else
    {
        /* Ignore an invalid integration pointer. */
    }
}

/** \brief Invalidates all L_HU_CarInfo signal-validity states. */
void LcanInput_NotifyHuCarInfoTimeout(void)
{
    LcanInput_Context.huCarInfo.avTailValid = FALSE;
    LcanInput_Context.huCarInfo.accValid = FALSE;
    LcanInput_Context.huCarInfo.ign1Valid = FALSE;
    LcanInput_Context.huCarInfo.ign3Valid = FALSE;
    LcanInput_Context.huCarInfo.rheostatLevelValid = FALSE;
    LcanInput_Context.huCarInfo.autoLightSensorValid = FALSE;
    LcanInput_Context.huCarInfo.detentOutputStateValid = FALSE;
    LcanInput_Context.huCarInfo.terminalControlStateValid = FALSE;
    LcanInput_Context.huCarInfo.driverSeatOccupancyValid = FALSE;
    LcanInput_Context.huCarInfo.driverDoorSwitchValid = FALSE;
    LcanInput_Context.huCarInfo.faceDetectionStateValid = FALSE;
    LcanInput_Context.huCarInfo.admValid = FALSE;
    LcanInput_Context.huCarInfo.welcomeStartRequestValid = FALSE;
    LcanInput_Context.huCarInfo.driverDoorUnlockStateValid = FALSE;
    LcanInput_Context.huCarInfo.engineStateValid = FALSE;
    LcanInput_Context.huCarInfo.hevReadyStateValid = FALSE;
    LcanInput_Context.huCarInfo.evDriveReadyStateValid = FALSE;
    LcanInput_UpdateBrightnessRequest();
}

/** \brief Invalidates all L_HU_MoodInfo signal-validity states. */
void LcanInput_NotifyHuMoodInfoTimeout(void)
{
    LcanInput_Context.huMoodInfo.slaveColorXValid = FALSE;
    LcanInput_Context.huMoodInfo.slaveColorYValid = FALSE;
    LcanInput_Context.huMoodInfo.slaveBrightnessValid = FALSE;
    LcanInput_Context.huMoodInfo.fadeInTimeValid = FALSE;
    LcanInput_Context.huMoodInfo.fadeOutTimeValid = FALSE;
    LcanInput_Context.huMoodInfo.fadeInOutEnabledValid = FALSE;
}

/** \brief Invalidates the RTE-level HU power-state snapshot. */
void LcanInput_NotifyHuStateTimeout(void)
{
    LcanInput_Context.huState.pwrStateValid = FALSE;
    LcanInput_Context.huState.hardResetAvailableValid = FALSE;
    LcanInput_Context.huState.resetPreWarningValid = FALSE;
    LcanInput_Context.huState.resetRequestValid = FALSE;
    LcanInput_Context.huState.autoBrightnessStateValid = FALSE;
    LcanInput_Context.brightnessControl.requestPending = FALSE;
    LcanInput_UpdateBrightnessRequest();
}

/** \brief Invalidates all L_HU_SysInfo signal-validity states. */
void LcanInput_NotifyHuSysInfoTimeout(void)
{
    LcanInput_Context.huSysInfo.hapticFeedbackLevelValid = FALSE;
    LcanInput_Context.huSysInfo.rotateChangeRequestValid = FALSE;
}

/*******************************************************************************
    10. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Resolves LCAN illumination priority and stores the latest request. */
static void LcanInput_UpdateBrightnessRequest(void)
{
    uint8 brightnessStep = (uint8)0U;

    if (LcanInput_GetRequestedBrightness(&brightnessStep) == TRUE)
    {
        LcanInput_Context.brightnessControl.requestedStep =
            brightnessStep;
        LcanInput_Context.brightnessControl.outputEnablePending = TRUE;

        if ((LcanInput_Context.brightnessControl.lastAcceptedStepValid ==
             FALSE) ||
            (LcanInput_Context.brightnessControl.lastAcceptedStep !=
             brightnessStep))
        {
            LcanInput_Context.brightnessControl.requestPending = TRUE;
        }
        else
        {
            LcanInput_Context.brightnessControl.requestPending = FALSE;
        }
    }
    else
    {
        LcanInput_Context.brightnessControl.requestPending = FALSE;
        LcanInput_Context.brightnessControl.outputEnablePending = FALSE;
    }
}

/** \brief Retries the latest HU brightness request until it is accepted. */
static void LcanInput_ProcessBrightnessRequest(void)
{
    const Led_ContextType *ledContext;
    uint8 requestedStep;
    Std_ReturnType result;

    if ((SWC_PWR_MGR_IsOperational() == TRUE) &&
        (LcanInput_Context.brightnessControl.requestPending == TRUE))
    {
        requestedStep =
            LcanInput_Context.brightnessControl.requestedStep;
        result = LedApp_RequestSetBrightness(requestedStep);

        if (result == STD_OK)
        {
            LcanInput_Context.brightnessControl.lastAcceptedStep =
                requestedStep;
            LcanInput_Context.brightnessControl.lastAcceptedStepValid = TRUE;

            if (LcanInput_Context.brightnessControl.requestedStep ==
                requestedStep)
            {
                LcanInput_Context.brightnessControl.requestPending = FALSE;
            }
            else
            {
                /* A newer request remains pending for the next cycle. */
            }
        }
        else
        {
            /* Retry while the LED FSM is busy or not in NORMAL. */
        }
    }
    else
    {
        /* No eligible brightness request is pending. */
    }

    ledContext = LedApp_GetContext();
    if ((SWC_PWR_MGR_IsOperational() == TRUE) &&
        (LcanInput_Context.brightnessControl.outputEnablePending == TRUE) &&
        (LcanInput_Context.brightnessControl.requestPending == FALSE) &&
        (ledContext->state == LED_STATE_NORMAL) &&
        (ledContext->settingRequestPending == FALSE))
    {
        if (LedApp_IsOutputEnabled() == TRUE)
        {
            LcanInput_Context.brightnessControl.outputEnablePending = FALSE;
        }
        else if (LedApp_EnableOutput() == STD_OK)
        {
            LcanInput_Context.brightnessControl.outputEnablePending = FALSE;
        }
        else
        {
            /* Retry after LED initialization or setting completion. */
        }
    }
    else
    {
        /* Wait for power operation and LED setting completion. */
    }
}

/** \brief Selects detent override or the latest valid automatic brightness. */
static boolean LcanInput_GetRequestedBrightness(
    uint8 * const brightnessStep)
{
    uint16 autoBrightness;
    boolean result = FALSE;

    if ((brightnessStep != NULL_PTR) &&
        (LcanInput_Context.huCarInfo.detentOutputStateValid == TRUE) &&
        (LcanInput_Context.huCarInfo.detentOutputState ==
         LCAN_INPUT_DETENT_OUTPUT_ON))
    {
        *brightnessStep = LED_BRIGHTNESS_STEP_FULL;
        result = TRUE;
    }
    else if ((brightnessStep != NULL_PTR) &&
             (LcanInput_Context.huState.autoBrightnessStateValid == TRUE))
    {
        autoBrightness =
            LcanInput_Context.huState.autoBrightnessState;

        if (autoBrightness == (uint16)0U)
        {
            *brightnessStep = LCAN_INPUT_AUTOBRIGHT_ZERO_STEP;
            result = TRUE;
        }
        else if ((autoBrightness >= (uint16)LED_BRIGHTNESS_STEP_MIN) &&
                 (autoBrightness <= (uint16)LED_BRIGHTNESS_STEP_MAX))
        {
            *brightnessStep = (uint8)autoBrightness;
            result = TRUE;
        }
        else
        {
            /* Automatic-brightness step 1 and values above 200 are ignored. */
        }
    }
    else
    {
        /* No valid detent or automatic-brightness signal is available. */
    }

    return result;
}
