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
    \file   SWC_PWR_MGR.c
    \brief  Adapts AUTOSAR RTE services to the power application.
            This module forwards valid ACC ADC and battery-voltage samples,
            applies the requested MPQ4323 output, and drives LCAN sleep levels.
********************************************************************************
|-------------------------------------------------------------------------------
|    3. A U T H O R   I D E N T I T Y
|-------------------------------------------------------------------------------
| Initials     Name                      Company
| --------     ---------------------     ---------------------------------------
| Ck           choi kyounghum            Mobaseelectronic software design team
| DL           lee juhwan(DaigerLee)     Mobaseelectronic software design team
|
|-------------------------------------------------------------------------------
|               R E V I S I O N   H I S T O R Y
|-------------------------------------------------------------------------------
| Date       Ver        Author  Description
| ---------  ---        ------  ------------------------------------------------
| 2025-03-10 000.01     DL      Creation
| 2026-10-21 000.02     DL      Standardized C source documentation
| 2026-10-21 000.03     DL      Integrated the power application FSM
| 2026-10-21 000.04     DL      Separated RTE and application processing
| 2026-10-21 000.05     DL      Moved ADC acquisition to the 5 ms runnable
| 2026-10-27 000.06     DL      Added guarded initialization in 10 ms runnable
| 2026-10-29 000.07     DL      Added controlled LCAN sleep-entry integration
| 2026-11-12 000.08     DL      Added external power status and event APIs
| 2026-11-13 000.09     DL      Added guarded LCAN wake-latch processing
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_PWR_MGR.h"

#define SWC_PWR_MGR_IMPLEMENTATION
#include "SWC_PWR_MGR.h"
#undef SWC_PWR_MGR_IMPLEMENTATION
#include "prj_typedefs.h"

#include "fsm_timer.h"
#include "lcan_comm.h"
#include "power_app.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** MPQ4323 digital-output level used when power is requested. */
#define PWR_MGR_OUTPUT_ON  ((IoHwAb_LevelType)1U)

/** MPQ4323 digital-output level used when power is not requested. */
#define PWR_MGR_OUTPUT_OFF ((IoHwAb_LevelType)0U)

/** LCAN EN level used with STB Off to request the INH-Low condition. */
#define PWR_MGR_LCAN_EN_INH_LOW_LEVEL  ((IoHwAb_LevelType)1U)

/** LCAN STB level used with EN On to request the INH-Low condition. */
#define PWR_MGR_LCAN_STB_INH_LOW_LEVEL ((IoHwAb_LevelType)0U)

/** Delay between repeated LCAN NoCom requests while FullCom remains active. */
#define PWR_MGR_LCAN_NOCOM_RETRY_TICKS ((uint32)200U)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Internal progress flags for the asynchronous LCAN sleep-entry sequence. */
typedef struct
{
    boolean noComRequested;
    boolean sleepReadyPosted;
} SwcPwrMgr_LcanSleepContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are defined. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Indicates that the power application has been initialized. */
static boolean SwcPwrMgr_Initialized = FALSE;

/** Runtime progress of the LCAN sleep-entry procedure. */
static SwcPwrMgr_LcanSleepContextType SwcPwrMgr_LcanSleepContext =
{
    FALSE,
    FALSE
};

/** Prevents LCAN sleep outputs while a received frame is waking the ECU. */
static volatile boolean SwcPwrMgr_LcanWakePending = FALSE;

/** Indicates that the pending wake request has been queued to the power FSM. */
static boolean SwcPwrMgr_LcanWakeEventQueued = FALSE;

/** Most recent HU power state received before it could be delivered. */
static volatile uint8 SwcPwrMgr_PendingHuPowerState =
    (uint8)POWER_HU_STATE_INVALID;

/** Indicates that one HU power-state value is waiting for delivery. */
static volatile boolean SwcPwrMgr_HuPowerStatePending = FALSE;

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void SwcPwrMgr_EnsureInitialized(void);
static void SwcPwrMgr_QueuePendingLcanWake(void);
static void SwcPwrMgr_ProcessPendingHuPowerState(void);
static void SwcPwrMgr_ControlLcanPowerMode(void);
static void SwcPwrMgr_HandleLcanSleep(boolean sleepReadyRequired);
static void SwcPwrMgr_ProcessSleepComMode(
    LcanCom_ModeType lcanComMode,
    boolean sleepReadyRequired);
static void SwcPwrMgr_RequestLcanNoCom(void);
static void SwcPwrMgr_CompleteLcanSleep(boolean sleepReadyRequired);
static void SwcPwrMgr_RestoreLcanFullCom(void);
static void SwcPwrMgr_ReleaseLcanWakeLatch(void);
static void SwcPwrMgr_ApplyLcanSleepOutputs(void);
static void SwcPwrMgr_ResetLcanSleepProgress(void);

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_PWR_MGR_START_SEC_CODE
#include "SWC_PWR_MGR_MemMap.h"

/** \brief Reports LCAN message reception to the power application. */
Std_ReturnType SWC_PWR_MGR_NotifyCanRx(void)
{
    SwcPwrMgr_LcanWakePending = TRUE;
    SwcPwrMgr_QueuePendingLcanWake();

    return STD_OK;
}

/** \brief Supplies one received HU power-state value. */
Std_ReturnType SWC_PWR_MGR_NotifyHuPowerState(uint8 huPowerState)
{
    Std_ReturnType result = STD_NOT_OK;

    if (huPowerState < (uint8)POWER_HU_STATE_MAX)
    {
        SwcPwrMgr_PendingHuPowerState = huPowerState;
        SwcPwrMgr_HuPowerStatePending = TRUE;

        if (SwcPwrMgr_Initialized == TRUE)
        {
            result = PowerApp_NotifyHuPowerState(huPowerState);
            if (result == STD_OK)
            {
                SwcPwrMgr_HuPowerStatePending = FALSE;
            }
            else
            {
                /* Retain the latest value for retry by the 10 ms runnable. */
            }
        }
        else
        {
            result = STD_OK;
        }
    }
    else
    {
        /* Reject values outside Power_HuStateType. */
    }

    return result;
}

/** \brief Requests the safety-mode power transition. */
Std_ReturnType SWC_PWR_MGR_RequestSafetyMode(void)
{
    return PowerApp_RequestSafetyMode();
}

/** \brief Returns whether the power FSM is operational. */
boolean SWC_PWR_MGR_IsOperational(void)
{
    return PowerApp_IsOperational();
}

/** \brief Returns the confirmed ACC state after hysteresis processing. */
boolean SWC_PWR_MGR_IsAccOn(void)
{
    return PowerApp_IsAccOn();
}

/** \brief Returns the confirmed battery operating-voltage state. */
boolean SWC_PWR_MGR_IsBatteryOperationOn(void)
{
    return PowerApp_IsBatteryOperationOn();
}

/** \brief Returns the requested external power-output state. */
boolean SWC_PWR_MGR_IsOutputEnabled(void)
{
    return PowerApp_IsOutputEnabled();
}

/** \brief Returns the latest synchronized power FSM state. */
Power_StateType SWC_PWR_MGR_GetState(void)
{
    return PowerApp_GetState();
}

/** \brief Returns read-only access to the power application context. */
const Power_ContextType * SWC_PWR_MGR_GetContext(void)
{
    return PowerApp_GetContext();
}

/**
 * \brief Executes power processing and applies the requested RTE output.
 *
 * ADC acquisition is performed by the 5 ms runnable. This runnable executes
 * the 10 ms power application and writes its MPQ4323 enable request.
 */
FUNC(void, SWC_PWR_MGR_CODE) RE_PWR_MGR_10ms_Task(void)
{
    IoHwAb_LevelType outputLevel;

    SwcPwrMgr_EnsureInitialized();

    if (SwcPwrMgr_Initialized == TRUE)
    {
        SwcPwrMgr_QueuePendingLcanWake();
        SwcPwrMgr_ProcessPendingHuPowerState();
        PowerApp_MainFunction10ms();
        SwcPwrMgr_QueuePendingLcanWake();
        SwcPwrMgr_ControlLcanPowerMode();

        if (PowerApp_IsOutputEnabled() == TRUE)
        {
            outputLevel = PWR_MGR_OUTPUT_ON;
        }
        else
        {
            outputLevel = PWR_MGR_OUTPUT_OFF;
        }

        (void)Rte_Call_R_DigDir_MPQ4323_EN_Output_WriteDirect(outputLevel);
    }
}

/**
 * \brief Acquires ACC and battery ADC values through the RTE.
 *
 * Each successfully read sample is converted to a project type and forwarded
 * to the power application. A failed read leaves the previous sample valid.
 */
FUNC(void, SWC_PWR_MGR_CODE) RE_PWR_MGR_5ms_Task(void)
{
    IoHwAb_ValueType accVoltage = (IoHwAb_ValueType)0U;
    IoHwAb_ValueType batteryVoltage = (IoHwAb_ValueType)0U;

    if (SwcPwrMgr_Initialized == TRUE)
    {
        if (Rte_Call_R_AnaInDir_AccVoltage_ReadDirect(&accVoltage, 1U) == E_OK)
        {
            PowerApp_SetAccVoltage((uint32)accVoltage);
        }
        else
        {
            /* The application retains its latest valid ACC sample. */
        }

        if (Rte_Call_R_AnaInDir_BatteryVoltage_ReadDirect(
                &batteryVoltage, 1U) == E_OK)
        {
            PowerApp_SetBatteryVoltage((uint32)batteryVoltage);
        }
        else
        {
            /* The application retains its latest valid battery sample. */
        }
    }
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Initializes the power application once before cyclic execution. */
static void SwcPwrMgr_EnsureInitialized(void)
{
    if (SwcPwrMgr_Initialized == FALSE)
    {
        PowerApp_Init();
        SwcPwrMgr_Initialized = TRUE;
    }
    else
    {
        /* Initialization is performed only once. */
    }
}

/** \brief Queues a latched LCAN wake event after the power FSM is ready. */
static void SwcPwrMgr_QueuePendingLcanWake(void)
{
    Power_StateType state;
    Std_ReturnType result;

    if ((SwcPwrMgr_Initialized == TRUE) &&
        (SwcPwrMgr_LcanWakePending == TRUE) &&
        (SwcPwrMgr_LcanWakeEventQueued == FALSE))
    {
        state = PowerApp_GetState();
        if ((state == POWER_STATE_SLEEP) ||
            (state == POWER_STATE_SLEEP_IN))
        {
            result = PowerApp_NotifyCanRx();
            if (result == STD_OK)
            {
                SwcPwrMgr_LcanWakeEventQueued = TRUE;
            }
            else
            {
                /* Retry when the power FSM event queue has room. */
            }
        }
        else
        {
            /* Wake event queueing is required only in a sleep state. */
        }
    }
    else
    {
        /* No unqueued LCAN wake request is pending. */
    }
}

/** \brief Delivers an HU power state retained across power initialization. */
static void SwcPwrMgr_ProcessPendingHuPowerState(void)
{
    Std_ReturnType result;
    uint8 huPowerState;

    if ((SwcPwrMgr_Initialized == TRUE) &&
        (SwcPwrMgr_HuPowerStatePending == TRUE))
    {
        huPowerState = SwcPwrMgr_PendingHuPowerState;
        result = PowerApp_NotifyHuPowerState(huPowerState);
        if (result == STD_OK)
        {
            SwcPwrMgr_HuPowerStatePending = FALSE;
        }
        else
        {
            /* Retry after the power FSM event queue has room. */
        }
    }
    else
    {
        /* No deferred HU power-state value is waiting. */
    }
}

/** \brief Executes state-dependent LCAN sleep or wake integration. */
static void SwcPwrMgr_ControlLcanPowerMode(void)
{
    Power_StateType state = PowerApp_GetState();

    if (SwcPwrMgr_LcanWakePending == TRUE)
    {
        if ((state != POWER_STATE_SLEEP) &&
            (state != POWER_STATE_SLEEP_IN))
        {
            SwcPwrMgr_LcanWakeEventQueued = FALSE;
        }
        else
        {
            /* Retain the queued flag while the wake transition is pending. */
        }

        SwcPwrMgr_ResetLcanSleepProgress();
        SwcPwrMgr_RestoreLcanFullCom();
        SwcPwrMgr_ReleaseLcanWakeLatch();
    }
    else
    {
        switch (state)
        {
            case POWER_STATE_SLEEP:
                SwcPwrMgr_HandleLcanSleep(TRUE);
                break;

            case POWER_STATE_SLEEP_IN:
                SwcPwrMgr_HandleLcanSleep(FALSE);
                break;

            case POWER_STATE_INIT:
            case POWER_STATE_STANDBY:
            case POWER_STATE_POWER_ON:
            case POWER_STATE_POWER_OFF:
            case POWER_STATE_ERROR:
                SwcPwrMgr_ResetLcanSleepProgress();
                SwcPwrMgr_RestoreLcanFullCom();
                break;

            default:
                SwcPwrMgr_ResetLcanSleepProgress();
                break;
        }
    }
}

/** \brief Requests NoCom and applies the LCAN sleep-entry procedure. */
static void SwcPwrMgr_HandleLcanSleep(boolean sleepReadyRequired)
{
    LcanCom_ModeType lcanComMode = LCAN_COM_MODE_INVALID;
    Std_ReturnType result;

    result = LcanCom_GetCurrentMode(&lcanComMode);
    if (result == E_OK)
    {
        SwcPwrMgr_ProcessSleepComMode(lcanComMode,
                                       sleepReadyRequired);
    }
    else
    {
        /* A failed mode read is retried on the next cycle. */
    }
}

/** \brief Processes the confirmed LCAN mode along the sleep-entry path. */
static void SwcPwrMgr_ProcessSleepComMode(
    LcanCom_ModeType lcanComMode,
    boolean sleepReadyRequired)
{
    switch (lcanComMode)
    {
        case LCAN_COM_MODE_FULL_COM:
        case LCAN_COM_MODE_SILENT_COM:
            SwcPwrMgr_RequestLcanNoCom();
            break;

        case LCAN_COM_MODE_NO_COM:
            SwcPwrMgr_CompleteLcanSleep(sleepReadyRequired);
            break;

        case LCAN_COM_MODE_INVALID:
        default:
            /* An invalid communication mode cannot complete sleep. */
            break;
    }
}

/** \brief Issues the initial or timed 200 ms retry of the NoCom request. */
static void SwcPwrMgr_RequestLcanNoCom(void)
{
    boolean requestRequired = FALSE;
    Std_ReturnType result;

    if (SwcPwrMgr_LcanSleepContext.noComRequested == FALSE)
    {
        requestRequired = TRUE;
    }
    else if (FSM_Timer_IsExpired(
                 FSM_TIMER_POWER_LCAN_SLEEP_RETRY) == TRUE)
    {
        requestRequired = TRUE;
    }
    else
    {
        /* Wait for NoCom confirmation or retry expiration. */
    }

    if (requestRequired == TRUE)
    {
        result = LcanCom_RequestMode(LCAN_COM_MODE_NO_COM);
        if (result == E_OK)
        {
            FSM_Timer_Start(FSM_TIMER_POWER_LCAN_SLEEP_RETRY,
                            PWR_MGR_LCAN_NOCOM_RETRY_TICKS);
            SwcPwrMgr_LcanSleepContext.noComRequested = TRUE;
        }
        else
        {
            /* Retry the request on the next 10 ms invocation. */
        }
    }
    else
    {
        /* No request is due in this invocation. */
    }
}

/** \brief Applies transceiver sleep levels and queues SLEEP completion once. */
static void SwcPwrMgr_CompleteLcanSleep(boolean sleepReadyRequired)
{
    Std_ReturnType result;

    FSM_Timer_Stop(FSM_TIMER_POWER_LCAN_SLEEP_RETRY);
    SwcPwrMgr_LcanSleepContext.noComRequested = FALSE;
    SwcPwrMgr_ApplyLcanSleepOutputs();

    if ((sleepReadyRequired == TRUE) &&
        (SwcPwrMgr_LcanSleepContext.sleepReadyPosted == FALSE))
    {
        result = PowerApp_RequestSleepReady();
        if (result == STD_OK)
        {
            SwcPwrMgr_LcanSleepContext.sleepReadyPosted = TRUE;
        }
        else
        {
            /* A full FSM queue is retried on the next cycle. */
        }
    }
    else if (sleepReadyRequired == TRUE)
    {
        /* Sleep completion has already been queued. */
    }
    else
    {
        /* The FSM is already in SLEEP_IN; no completion event is required. */
    }
}

/** \brief Requests FullCom when the power state leaves its sleep path. */
static void SwcPwrMgr_RestoreLcanFullCom(void)
{
    LcanCom_ModeType currentMode = LCAN_COM_MODE_INVALID;
    LcanCom_ModeType requestedMode = LCAN_COM_MODE_INVALID;
    Std_ReturnType result;

    result = LcanCom_GetCurrentMode(&currentMode);
    if (result == E_OK)
    {
        if (currentMode != LCAN_COM_MODE_FULL_COM)
        {
            result = LcanCom_GetRequestedMode(&requestedMode);
            if ((result != E_OK) ||
                (requestedMode != LCAN_COM_MODE_FULL_COM))
            {
                (void)LcanCom_RequestMode(LCAN_COM_MODE_FULL_COM);
            }
            else
            {
                /* The FullCom transition has already been requested. */
            }
        }
        else
        {
            /* FullCom is already applied. */
        }
    }
    else
    {
        /* A failed mode read is retried on the next cycle. */
    }
}

/** \brief Releases the wake latch after state and ComM wake confirmation. */
static void SwcPwrMgr_ReleaseLcanWakeLatch(void)
{
    Power_StateType state = PowerApp_GetState();
    LcanCom_ModeType currentMode = LCAN_COM_MODE_INVALID;
    boolean awakeState = FALSE;

    switch (state)
    {
        case POWER_STATE_STANDBY:
        case POWER_STATE_POWER_ON:
        case POWER_STATE_POWER_OFF:
            awakeState = TRUE;
            break;

        case POWER_STATE_INIT:
        case POWER_STATE_SLEEP:
        case POWER_STATE_SLEEP_IN:
        case POWER_STATE_ERROR:
        default:
            awakeState = FALSE;
            break;
    }

    if ((awakeState == TRUE) &&
        (LcanCom_GetCurrentMode(&currentMode) == E_OK) &&
        (currentMode == LCAN_COM_MODE_FULL_COM))
    {
        SwcPwrMgr_LcanWakePending = FALSE;
        SwcPwrMgr_LcanWakeEventQueued = FALSE;
    }
    else
    {
        /* Keep the latch until both state and FullCom are confirmed. */
    }
}

/** \brief Applies the transceiver levels used to request LCAN INH Low. */
static void SwcPwrMgr_ApplyLcanSleepOutputs(void)
{
    (void)Rte_Call_R_DigDir_LCAN_EN_WriteDirect(
        PWR_MGR_LCAN_EN_INH_LOW_LEVEL);
    (void)Rte_Call_R_DigDir_LCAN_STB_WriteDirect(
        PWR_MGR_LCAN_STB_INH_LOW_LEVEL);
}

/** \brief Clears sleep-entry progress when the sequence is not active. */
static void SwcPwrMgr_ResetLcanSleepProgress(void)
{
    FSM_Timer_Stop(FSM_TIMER_POWER_LCAN_SLEEP_RETRY);
    SwcPwrMgr_LcanSleepContext.noComRequested = FALSE;
    SwcPwrMgr_LcanSleepContext.sleepReadyPosted = FALSE;
}

#define SWC_PWR_MGR_STOP_SEC_CODE
#include "SWC_PWR_MGR_MemMap.h"
