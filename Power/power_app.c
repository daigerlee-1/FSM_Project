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
    \file   power_app.c
    \brief  Implements the event-driven power-management application FSM.
            This module evaluates ACC input, handles startup timing, controls
            dependent applications, and exposes the requested power output.
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
| 2026-10-21 000.03     DL      Added RTE-independent power I/O processing
| 2026-10-21 000.04     DL      Added battery-based CAN TX permission
| 2026-10-21 000.05     DL      Documented battery ADC calibration points
| 2026-10-21 000.06     DL      Added 100 ms FSM-timer hysteresis confirmation
| 2026-10-21 000.07     DL      Generalized battery operating-voltage status
| 2026-10-22 000.08     DL      Implemented CCP/RRC power-state diagram
| 2026-10-29 000.09     DL      Added LCAN sleep flow and aligned state values
| 2026-10-30 000.10     DL      Assigned LED enable control to LCAN input
| 2026-11-12 000.11     DL      Added confirmed ACC-state query API
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "power_app.h"

#include "fsm.h"
#include "fsm_timer.h"
#include "knob_led.h"
#if 0
#include "nvm_app.h"
#endif


/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Continuous threshold time expressed in central 1 ms timer ticks. */
#define POWER_HYSTERESIS_TIME_TICKS ((uint32)100U)

/** Delay before retrying a qualified event rejected by the FSM queue. */
#define POWER_EVENT_RETRY_TIME_TICKS ((uint32)10U)

/** HU no-message timeout while the power FSM is in STANDBY: 5 seconds. */
#define POWER_STANDBY_TIMEOUT_TICKS ((uint32)5000U)

/** HU no-message timeout while POWER_ON: 3 minutes and 5 seconds. */
#define POWER_ON_TIMEOUT_TICKS ((uint32)185000U)

/** HU no-message timeout while POWER_OFF: 5 minutes. */
#define POWER_OFF_TIMEOUT_TICKS ((uint32)300000U)

/** ACC ADC value corresponding to the 7.0 V ignition-ON threshold. */
#define POWER_ACC_ON_THRESHOLD_ADC  ((uint32)1450U)

/** ACC ADC value corresponding to the 6.5 V ignition-OFF threshold. */
#define POWER_ACC_OFF_THRESHOLD_ADC ((uint32)1352U)

/* Battery-voltage ADC calibration points retained for power policy use. */
/** Battery ADC raw value measured at 6.5 V. */
#define POWER_BAT_ADC_6V5 ((uint32)1335U)

/** Battery ADC raw value measured at 7.0 V. */
#define POWER_BAT_ADC_7V0 ((uint32)1440U)

/** Battery ADC raw value measured at 8.5 V. */
#define POWER_BAT_ADC_8V5 ((uint32)1745U)

/** Battery ADC raw value measured at 9.0 V. */
#define POWER_BAT_ADC_9V0 ((uint32)1852U)

/** Battery ADC raw value measured at 16.0 V. */
#define POWER_BAT_ADC_16V0 ((uint32)3260U)

/** Battery ADC raw value measured at 16.5 V. */
#define POWER_BAT_ADC_16V5 ((uint32)3372U)

/** Battery ADC raw value measured at 18.0 V. */
#define POWER_BAT_ADC_18V0 ((uint32)3675U)

/** Battery ADC raw value measured at 18.5 V. */
#define POWER_BAT_ADC_18V5 ((uint32)3768U)

/** Low threshold for turning battery operation ON: 7.0 V, ADC value 1440. */
#define POWER_BAT_OPERATION_LOW_ON_ADC  (POWER_BAT_ADC_7V0)

/** Low threshold for turning battery operation OFF: 6.5 V, ADC value 1335. */
#define POWER_BAT_OPERATION_LOW_OFF_ADC (POWER_BAT_ADC_6V5)

/** High threshold for turning battery operation ON: 18.0 V, ADC value 3675. */
#define POWER_BAT_OPERATION_HIGH_ON_ADC (POWER_BAT_ADC_18V0)

/** High threshold for turning battery operation OFF: 18.5 V, ADC value 3768. */
#define POWER_BAT_OPERATION_HIGH_OFF_ADC (POWER_BAT_ADC_18V5)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* FSM state-entry operations. */
static void PowerApp_EntryInit(void);
static void PowerApp_EntrySleep(void);
static void PowerApp_EntryStandby(void);
static void PowerApp_EntryPowerOn(void);
static void PowerApp_EntryPowerOff(void);
static void PowerApp_EntryError(void);
static void PowerApp_EntrySleepIn(void);

/* Common FSM state-exit and periodic operations. */
static void PowerApp_ExitNoAction(void);
static void PowerApp_DoNoAction(void);

/* FSM transition guards. */
static uint8 PowerApp_GuardAlways(void);
static uint8 PowerApp_GuardAccOff(void);
static uint8 PowerApp_GuardHuTimedOut(void);

/* FSM transition actions. */
static void PowerApp_ActionNone(void);
static void PowerApp_ActionReset(void);

/* Module-local input and output processing. */
static void PowerApp_UpdateAccEvent(void);
static boolean PowerApp_GetRequestedAccState(void);
static void PowerApp_ReportAccTransition(boolean requestedAccOn);
static void PowerApp_UpdateBatteryOperation(void);
static boolean PowerApp_GetRequestedBatteryOperation(void);
static void PowerApp_ConfirmBatteryOperation(boolean operationOn);
static void PowerApp_UpdateHysteresisTimer(
    FSM_TimerIdType timerId,
    boolean transitionCondition,
    boolean * const timerActive);
static void PowerApp_UpdateHuTimeout(void);
static Std_ReturnType PowerApp_AcceptHuPowerState(
    Power_HuStateType huPowerState,
    Power_EventType event);
static Std_ReturnType PowerApp_ReportHuTimeout(void);
static void PowerApp_UpdateOutputRequest(void);
static void PowerApp_StartHuTimeout(uint32 timeoutTicks);
static void PowerApp_RestartHuTimeout(void);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Runtime context owned by the power application module. */
static Power_ContextType PowerApp_Context;

/*******************************************************************************
    9. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/** State-indexed entry, exit, and periodic-operation table. */
static const Fsm_StateOperationType PowerApp_StateOperations[POWER_STATE_MAX] =
{
    { PowerApp_EntryInit,     PowerApp_ExitNoAction, PowerApp_DoNoAction },
    { PowerApp_EntrySleep,    PowerApp_ExitNoAction, PowerApp_DoNoAction },
    { PowerApp_EntrySleepIn,  PowerApp_ExitNoAction, PowerApp_DoNoAction },
    { PowerApp_EntryStandby,  PowerApp_ExitNoAction, PowerApp_DoNoAction },
    { PowerApp_EntryPowerOn,  PowerApp_ExitNoAction, PowerApp_DoNoAction },
    { PowerApp_EntryPowerOff, PowerApp_ExitNoAction, PowerApp_DoNoAction },
    { PowerApp_EntryError,    PowerApp_ExitNoAction, PowerApp_DoNoAction }
};

/** Event-driven state-transition table for the power FSM. */
static const Fsm_TransitionType PowerApp_Transitions[] =
{
    /* POWER_STATE_INIT transitions. */
    { (Fsm_StateType)POWER_STATE_INIT,
      (Fsm_EventType)POWER_EVENT_INIT_DONE,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_INIT,
      (Fsm_EventType)POWER_EVENT_FAULT,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_ERROR },

    /* POWER_STATE_SLEEP transitions. */
    { (Fsm_StateType)POWER_STATE_SLEEP,
      (Fsm_EventType)POWER_EVENT_ACC_ON,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_STANDBY },
    { (Fsm_StateType)POWER_STATE_SLEEP,
      (Fsm_EventType)POWER_EVENT_CAN_RX,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_STANDBY },
    { (Fsm_StateType)POWER_STATE_SLEEP,
      (Fsm_EventType)POWER_EVENT_HU_POWER_ON,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_ON },
    { (Fsm_StateType)POWER_STATE_SLEEP,
      (Fsm_EventType)POWER_EVENT_SLEEP_READY,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP_IN },
    { (Fsm_StateType)POWER_STATE_SLEEP,
      (Fsm_EventType)POWER_EVENT_FAULT,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_ERROR },

    /* POWER_STATE_SLEEP_IN transitions. */
    { (Fsm_StateType)POWER_STATE_SLEEP_IN,
      (Fsm_EventType)POWER_EVENT_ACC_ON,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_STANDBY },
    { (Fsm_StateType)POWER_STATE_SLEEP_IN,
      (Fsm_EventType)POWER_EVENT_CAN_RX,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_STANDBY },
    { (Fsm_StateType)POWER_STATE_SLEEP_IN,
      (Fsm_EventType)POWER_EVENT_HU_POWER_ON,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_ON },
    { (Fsm_StateType)POWER_STATE_SLEEP_IN,
      (Fsm_EventType)POWER_EVENT_FAULT,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_ERROR },

    /* POWER_STATE_STANDBY transitions. */
    { (Fsm_StateType)POWER_STATE_STANDBY,
      (Fsm_EventType)POWER_EVENT_HU_SLEEP,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_STANDBY,
      (Fsm_EventType)POWER_EVENT_HU_POWER_OFF,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_OFF },
    { (Fsm_StateType)POWER_STATE_STANDBY,
      (Fsm_EventType)POWER_EVENT_HU_POWER_ON,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_ON },
    { (Fsm_StateType)POWER_STATE_STANDBY,
      (Fsm_EventType)POWER_EVENT_HU_TIMEOUT,
      PowerApp_GuardAccOff, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_STANDBY,
      (Fsm_EventType)POWER_EVENT_ACC_OFF,
      PowerApp_GuardHuTimedOut, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_STANDBY,
      (Fsm_EventType)POWER_EVENT_FAULT,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_ERROR },

    /* POWER_STATE_POWER_ON transitions. */
    { (Fsm_StateType)POWER_STATE_POWER_ON,
      (Fsm_EventType)POWER_EVENT_HU_POWER_OFF,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_OFF },
    { (Fsm_StateType)POWER_STATE_POWER_ON,
      (Fsm_EventType)POWER_EVENT_HU_TIMEOUT,
      PowerApp_GuardAccOff, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_OFF },
    { (Fsm_StateType)POWER_STATE_POWER_ON,
      (Fsm_EventType)POWER_EVENT_ACC_OFF,
      PowerApp_GuardHuTimedOut, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_OFF },
    { (Fsm_StateType)POWER_STATE_POWER_ON,
      (Fsm_EventType)POWER_EVENT_SAFETY_MODE,
      PowerApp_GuardAccOff, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_POWER_ON,
      (Fsm_EventType)POWER_EVENT_FAULT,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_ERROR },

    /* POWER_STATE_POWER_OFF transitions. */
    { (Fsm_StateType)POWER_STATE_POWER_OFF,
      (Fsm_EventType)POWER_EVENT_HU_SLEEP,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_POWER_OFF,
      (Fsm_EventType)POWER_EVENT_HU_POWER_ON,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_POWER_ON },
    { (Fsm_StateType)POWER_STATE_POWER_OFF,
      (Fsm_EventType)POWER_EVENT_HU_TIMEOUT,
      PowerApp_GuardAccOff, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_POWER_OFF,
      (Fsm_EventType)POWER_EVENT_ACC_OFF,
      PowerApp_GuardHuTimedOut, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_SLEEP },
    { (Fsm_StateType)POWER_STATE_POWER_OFF,
      (Fsm_EventType)POWER_EVENT_FAULT,
      PowerApp_GuardAlways, PowerApp_ActionNone,
      (Fsm_StateType)POWER_STATE_ERROR },

    /* POWER_STATE_ERROR transitions. */
    { (Fsm_StateType)POWER_STATE_ERROR,
      (Fsm_EventType)POWER_EVENT_RESET,
      PowerApp_GuardAlways, PowerApp_ActionReset,
      (Fsm_StateType)POWER_STATE_INIT }
};

/** Number of entries in PowerApp_Transitions, derived at compile time. */
#define POWER_TRANSITION_COUNT                                      \
    ((uint16)(sizeof(PowerApp_Transitions) /                        \
              sizeof(PowerApp_Transitions[0])))

/** Static configuration supplied to the generic FSM core. */
static const Fsm_ConfigType PowerApp_Config =
{
    (Fsm_StateType)POWER_STATE_INIT,
    (Fsm_StateType)POWER_STATE_MAX,
    (Fsm_EventType)POWER_EVENT_MAX,
    PowerApp_StateOperations,
    PowerApp_Transitions,
    POWER_TRANSITION_COUNT
};

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes the power context and generic FSM instance. */
void PowerApp_Init(void)
{
    PowerApp_Context.accVoltage = (uint32)0U;
    PowerApp_Context.batteryVoltage = (uint32)0U;
    PowerApp_Context.accVoltageValid = FALSE;
    PowerApp_Context.batteryVoltageValid = FALSE;
    PowerApp_Context.accOn = FALSE;
    PowerApp_Context.outputEnabled = FALSE;
    PowerApp_Context.batteryOperationOn = FALSE;
    PowerApp_Context.accHysteresisTimerActive = FALSE;
    PowerApp_Context.batteryHysteresisTimerActive = FALSE;
    PowerApp_Context.huPowerState = POWER_HU_STATE_INVALID;
    PowerApp_Context.huPowerStateValid = FALSE;
    PowerApp_Context.huMessageTimedOut = FALSE;
    PowerApp_Context.huTimeoutTimerActive = FALSE;
    FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
    FSM_Timer_Stop(FSM_TIMER_POWER_ACC_HYSTERESIS);
    FSM_Timer_Stop(FSM_TIMER_POWER_BAT_HYSTERESIS);

    (void)Fsm_Init(&PowerApp_Context.fsm, &PowerApp_Config);
    PowerApp_Context.state = POWER_STATE_INIT;
    PowerApp_Context.event = POWER_EVENT_MAX;
    (void)Fsm_PostEvent(&PowerApp_Context.fsm,
                        (Fsm_EventType)POWER_EVENT_INIT_DONE);
}

/** \brief Executes voltage, HU-timeout, FSM, and output-request processing. */
void PowerApp_MainFunction10ms(void)
{
    PowerApp_UpdateAccEvent();
    PowerApp_UpdateBatteryOperation();

    PowerApp_UpdateHuTimeout();

    Fsm_Dispatch(&PowerApp_Context.fsm);
    Fsm_Do(&PowerApp_Context.fsm);
    PowerApp_Context.state =
        (Power_StateType)Fsm_GetState(&PowerApp_Context.fsm);
    PowerApp_Context.event =
        (Power_EventType)Fsm_GetLastEvent(&PowerApp_Context.fsm);
    PowerApp_UpdateOutputRequest();
}

/** \brief Stores one valid ACC ADC raw value from the integration layer. */
void PowerApp_SetAccVoltage(uint32 voltage)
{
    PowerApp_Context.accVoltage = voltage;
    PowerApp_Context.accVoltageValid = TRUE;
}

/** \brief Stores one valid battery ADC raw value for CAN TX policy. */
void PowerApp_SetBatteryVoltage(uint32 voltage)
{
    PowerApp_Context.batteryVoltage = voltage;
    PowerApp_Context.batteryVoltageValid = TRUE;
}

/** \brief Queues an ignition-ON event for asynchronous FSM dispatch. */
Std_ReturnType PowerApp_RequestIgnitionOn(void)
{
    Std_ReturnType result;

    result = Fsm_PostEvent(&PowerApp_Context.fsm,
                           (Fsm_EventType)POWER_EVENT_ACC_ON);
    if (result == STD_OK)
    {
        PowerApp_Context.accOn = TRUE;
    }
    else
    {
        /* Retain the accepted ACC state when queueing fails. */
    }

    return result;
}

/** \brief Queues an ignition-OFF event for asynchronous FSM dispatch. */
Std_ReturnType PowerApp_RequestIgnitionOff(void)
{
    Std_ReturnType result;

    result = Fsm_PostEvent(&PowerApp_Context.fsm,
                           (Fsm_EventType)POWER_EVENT_ACC_OFF);
    if (result == STD_OK)
    {
        PowerApp_Context.accOn = FALSE;
    }
    else
    {
        /* Retain the accepted ACC state when queueing fails. */
    }

    return result;
}

/** \brief Queues notification that any CAN frame was received. */
Std_ReturnType PowerApp_NotifyCanRx(void)
{
    Std_ReturnType result = STD_OK;

    Power_StateType state;

    state = (Power_StateType)Fsm_GetState(&PowerApp_Context.fsm);
    if ((state == POWER_STATE_SLEEP) ||
        (state == POWER_STATE_SLEEP_IN))
    {
        result = Fsm_PostEvent(&PowerApp_Context.fsm,
                               (Fsm_EventType)POWER_EVENT_CAN_RX);
    }
    else
    {
        /* CAN reception is a wake event only in a sleep-related state. */
    }

    return result;
}

/** \brief Stores and queues one HU power-state signal reception. */
Std_ReturnType PowerApp_NotifyHuPowerState(uint8 huPowerState)
{
    Std_ReturnType result = STD_NOT_OK;

    switch (huPowerState)
    {
        case (uint8)POWER_HU_STATE_SLEEP:
            result = PowerApp_AcceptHuPowerState(
                POWER_HU_STATE_SLEEP,
                POWER_EVENT_HU_SLEEP);
            break;

        case (uint8)POWER_HU_STATE_POWER_OFF:
            result = PowerApp_AcceptHuPowerState(
                POWER_HU_STATE_POWER_OFF,
                POWER_EVENT_HU_POWER_OFF);
            break;

        case (uint8)POWER_HU_STATE_POWER_ON:
            result = PowerApp_AcceptHuPowerState(
                POWER_HU_STATE_POWER_ON,
                POWER_EVENT_HU_POWER_ON);
            break;

        case (uint8)POWER_HU_STATE_INVALID:
            PowerApp_RestartHuTimeout();
            /* Invalid signal data keeps the previously accepted HU state. */
            result = STD_OK;
            break;

        default:
            result = STD_NOT_OK;
            break;
    }

    return result;
}

/** \brief Queues one externally reported HU power-state timeout. */
Std_ReturnType PowerApp_NotifyHuPowerStateTimeout(void)
{
    Std_ReturnType result;

    FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
    PowerApp_Context.huTimeoutTimerActive = FALSE;

    if (PowerApp_Context.huMessageTimedOut == FALSE)
    {
        result = PowerApp_ReportHuTimeout();
    }
    else
    {
        /* The current HU message-loss condition has already been reported. */
        result = STD_OK;
    }

    return result;
}

/** \brief Queues reception of the safety-mode test message. */
Std_ReturnType PowerApp_RequestSafetyMode(void)
{
    return Fsm_PostEvent(&PowerApp_Context.fsm,
                         (Fsm_EventType)POWER_EVENT_SAFETY_MODE);
}

/** \brief Queues a fault event for asynchronous FSM dispatch. */
Std_ReturnType PowerApp_RequestFault(void)
{
    return Fsm_PostEvent(&PowerApp_Context.fsm,
                         (Fsm_EventType)POWER_EVENT_FAULT);
}

/** \brief Queues a reset event for asynchronous FSM dispatch. */
Std_ReturnType PowerApp_RequestReset(void)
{
    return Fsm_PostEvent(&PowerApp_Context.fsm,
                         (Fsm_EventType)POWER_EVENT_RESET);
}

/** \brief Queues completion of the LCAN sleep-entry preparation. */
Std_ReturnType PowerApp_RequestSleepReady(void)
{
    return Fsm_PostEvent(&PowerApp_Context.fsm,
                         (Fsm_EventType)POWER_EVENT_SLEEP_READY);
}

/** \brief Reports whether the synchronized FSM state is POWER_ON. */
boolean PowerApp_IsOperational(void)
{
    boolean ret = FALSE;

    if (PowerApp_Context.state == POWER_STATE_POWER_ON)
    {
        ret = TRUE;
    }
    else
    {
        ret = FALSE;
    }

    return ret;
}

/** \brief Returns the external power-output request derived from FSM state. */
boolean PowerApp_IsOutputEnabled(void)
{
    return PowerApp_Context.outputEnabled;
}

/** \brief Reports the ACC state confirmed by hysteresis processing. */
boolean PowerApp_IsAccOn(void)
{
    return PowerApp_Context.accOn;
}

/** \brief Returns the latest synchronized power FSM state. */
Power_StateType PowerApp_GetState(void)
{
    return PowerApp_Context.state;
}

/** \brief Returns read-only access to the module-owned power context. */
const Power_ContextType * PowerApp_GetContext(void)
{
    return &PowerApp_Context;
}

/** \brief Reports whether the battery is inside its operating-voltage range. */
boolean PowerApp_IsBatteryOperationOn(void)
{
    return PowerApp_Context.batteryOperationOn;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Initializes outputs while the FSM waits to enter SLEEP. */
static void PowerApp_EntryInit(void)
{
    FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
    PowerApp_Context.huMessageTimedOut = FALSE;
    PowerApp_Context.huTimeoutTimerActive = FALSE;

    /* DrvConsole_WriteString("[POWER] state INIT\r\n"); */
}

/** \brief Starts the system-OFF and LCAN sleep preparation policy. */
static void PowerApp_EntrySleep(void)
{
    FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
    PowerApp_Context.huMessageTimedOut = FALSE;
    PowerApp_Context.huTimeoutTimerActive = FALSE;

    LedApp_DisableOutput();

#if (NVM_FUNCTION_ENABLE == 1U)
    (void)NvmApp_RequestWrite((uint8)POWER_STATE_SLEEP);
#endif

    /* DrvConsole_WriteString("[POWER] state SLEEP\r\n"); */
}

/** \brief Enables communication and starts the 5-second HU timeout. */
static void PowerApp_EntryStandby(void)
{
    PowerApp_StartHuTimeout(POWER_STANDBY_TIMEOUT_TICKS);

#if (NVM_FUNCTION_ENABLE == 1U)
    (void)NvmApp_RequestWrite((uint8)POWER_STATE_STANDBY);
#endif
    /* DrvConsole_WriteString("[POWER] state STANDBY\r\n"); */
}

/** \brief Enables system power and starts the 3-minute-5-second timeout. */
static void PowerApp_EntryPowerOn(void)
{
    PowerApp_StartHuTimeout(POWER_ON_TIMEOUT_TICKS);

#if (NVM_FUNCTION_ENABLE == 1U)
    (void)NvmApp_RequestWrite((uint8)POWER_STATE_POWER_ON);
#endif
    /* DrvConsole_WriteString("[POWER] state POWER_ON\r\n"); */
}

/** \brief Disables controlled power and starts the 5-minute timeout. */
static void PowerApp_EntryPowerOff(void)
{
    PowerApp_StartHuTimeout(POWER_OFF_TIMEOUT_TICKS);

#if (NVM_FUNCTION_ENABLE == 1U)
    (void)NvmApp_RequestWrite((uint8)POWER_STATE_POWER_OFF);
#endif
    /* DrvConsole_WriteString("[POWER] state POWER_OFF\r\n"); */
}

/** \brief Applies fail-safe outputs and stops HU timeout monitoring. */
static void PowerApp_EntryError(void)
{
    FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
    PowerApp_Context.huMessageTimedOut = FALSE;
    PowerApp_Context.huTimeoutTimerActive = FALSE;

    /* (void)LedApp_RequestFault(); */

#if (NVM_FUNCTION_ENABLE == 1U)
    (void)NvmApp_RequestWrite((uint8)POWER_STATE_ERROR);
#endif
    /* DrvConsole_WriteString("[POWER] state ERROR\r\n"); */
}

/** \brief Applies the final low-current state after LCAN sleep preparation. */
static void PowerApp_EntrySleepIn(void)
{
    FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
    PowerApp_Context.huMessageTimedOut = FALSE;
    PowerApp_Context.huTimeoutTimerActive = FALSE;
    /* DrvConsole_WriteString("[POWER] state SLEEP_IN\r\n"); */
}

/** \brief Provides the common no-operation state-exit callback. */
static void PowerApp_ExitNoAction(void)
{
    /* No common power-state exit processing is required. */
}

/** \brief Provides the no-operation callback for event-driven states. */
static void PowerApp_DoNoAction(void)
{
    /* Power transitions are event driven. */
}

/** \brief Allows every transition that references this guard. */
static uint8 PowerApp_GuardAlways(void)
{
    return STD_TRUE;
}

/** \brief Allows a timeout-based transition only while ACC is OFF. */
static uint8 PowerApp_GuardAccOff(void)
{
    uint8 result = STD_FALSE;

    if (PowerApp_Context.accOn == FALSE)
    {
        result = STD_TRUE;
    }
    else
    {
        result = STD_FALSE;
    }

    return result;
}

/** \brief Allows an ACC-OFF transition after HU timeout has elapsed. */
static uint8 PowerApp_GuardHuTimedOut(void)
{
    uint8 result = STD_FALSE;

    if (PowerApp_Context.huMessageTimedOut == TRUE)
    {
        result = STD_TRUE;
    }
    else
    {
        result = STD_FALSE;
    }

    return result;
}

/** \brief Provides the no-operation transition action. */
static void PowerApp_ActionNone(void)
{
    /* Target-state entry functions apply the required policy. */
}

/** \brief Resets dependent applications and schedules INIT-to-SLEEP startup. */
static void PowerApp_ActionReset(void)
{
    FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);

    /* (void)LedApp_RequestReset(); */

#if (NVM_FUNCTION_ENABLE == 1U)
    (void)NvmApp_RequestReset();
#endif
    (void)Fsm_PostEvent(&PowerApp_Context.fsm,
                        (Fsm_EventType)POWER_EVENT_INIT_DONE);
}

/**
 * \brief Converts a valid ACC sample change into one queued FSM event.
 *
 * The accepted ACC state changes only after the corresponding event has been
 * queued successfully, allowing a full queue to be retried on the next cycle.
 */
static void PowerApp_UpdateAccEvent(void)
{
    boolean requestedAccOn = PowerApp_Context.accOn;
    boolean transitionCondition = FALSE;

    if (PowerApp_Context.accVoltageValid == TRUE)
    {
        requestedAccOn = PowerApp_GetRequestedAccState();
        if (requestedAccOn != PowerApp_Context.accOn)
        {
            transitionCondition = TRUE;
        }
        else
        {
            /* The sample does not request a change from the accepted state. */
        }
    }
    else
    {
        /* Ignore ACC processing until the first valid sample is provided. */
    }

    PowerApp_UpdateHysteresisTimer(
        FSM_TIMER_POWER_ACC_HYSTERESIS,
        transitionCondition,
        &PowerApp_Context.accHysteresisTimerActive);

    if (transitionCondition == TRUE)
    {
        PowerApp_ReportAccTransition(requestedAccOn);
    }
    else
    {
        /* No qualified ACC state transition is pending. */
    }
}

/** \brief Derives the requested ACC state from the configured thresholds. */
static boolean PowerApp_GetRequestedAccState(void)
{
    boolean requestedAccOn = PowerApp_Context.accOn;

    if ((PowerApp_Context.accOn == FALSE) &&
        (PowerApp_Context.accVoltage >= POWER_ACC_ON_THRESHOLD_ADC))
    {
        requestedAccOn = TRUE;
    }
    else if ((PowerApp_Context.accOn == TRUE) &&
             (PowerApp_Context.accVoltage <= POWER_ACC_OFF_THRESHOLD_ADC))
    {
        requestedAccOn = FALSE;
    }
    else
    {
        /* Retain the accepted ACC state inside the threshold dead band. */
    }

    return requestedAccOn;
}

/** \brief Queues a confirmed ACC change or schedules an event retry. */
static void PowerApp_ReportAccTransition(boolean requestedAccOn)
{
    Std_ReturnType result;

    if (FSM_Timer_IsExpired(FSM_TIMER_POWER_ACC_HYSTERESIS) == TRUE)
    {
        if (requestedAccOn == TRUE)
        {
            result = PowerApp_RequestIgnitionOn();
        }
        else
        {
            result = PowerApp_RequestIgnitionOff();
        }

        if (result == STD_OK)
        {
            PowerApp_Context.accOn = requestedAccOn;
            PowerApp_Context.accHysteresisTimerActive = FALSE;
        }
        else
        {
            FSM_Timer_Start(FSM_TIMER_POWER_ACC_HYSTERESIS,
                            POWER_EVENT_RETRY_TIME_TICKS);
            PowerApp_Context.accHysteresisTimerActive = TRUE;
        }
    }
    else
    {
        /* Wait until the ACC confirmation or retry timer expires. */
    }
}

/**
 * \brief Updates the common battery operating-voltage state.
 *
 * Operation turns ON after the battery remains between 7.0 V and 18.0 V for
 * the configured confirmation time. Once ON, operation turns OFF after the
 * battery remains at or below 6.5 V or at or above 18.5 V for that time.
 */
static void PowerApp_UpdateBatteryOperation(void)
{
    boolean operationOn = PowerApp_Context.batteryOperationOn;
    boolean transitionCondition = FALSE;

    if (PowerApp_Context.batteryVoltageValid == TRUE)
    {
        operationOn = PowerApp_GetRequestedBatteryOperation();
        if (operationOn != PowerApp_Context.batteryOperationOn)
        {
            transitionCondition = TRUE;
        }
        else
        {
            /* The sample does not request a change from the accepted state. */
        }
    }
    else
    {
        PowerApp_Context.batteryOperationOn = FALSE;
    }

    PowerApp_UpdateHysteresisTimer(
        FSM_TIMER_POWER_BAT_HYSTERESIS,
        transitionCondition,
        &PowerApp_Context.batteryHysteresisTimerActive);

    if (transitionCondition == TRUE)
    {
        PowerApp_ConfirmBatteryOperation(operationOn);
    }
    else
    {
        /* Retain the current permission until a change is confirmed. */
    }
}

/** \brief Derives the requested battery operation state from its thresholds. */
static boolean PowerApp_GetRequestedBatteryOperation(void)
{
    boolean operationOn = PowerApp_Context.batteryOperationOn;

    if ((PowerApp_Context.batteryOperationOn == FALSE) &&
        (PowerApp_Context.batteryVoltage >= POWER_BAT_OPERATION_LOW_ON_ADC) &&
        (PowerApp_Context.batteryVoltage <= POWER_BAT_OPERATION_HIGH_ON_ADC))
    {
        operationOn = TRUE;
    }
    else if ((PowerApp_Context.batteryOperationOn == TRUE) &&
             ((PowerApp_Context.batteryVoltage <=
               POWER_BAT_OPERATION_LOW_OFF_ADC) ||
              (PowerApp_Context.batteryVoltage >=
               POWER_BAT_OPERATION_HIGH_OFF_ADC)))
    {
        operationOn = FALSE;
    }
    else
    {
        /* Retain the accepted operation state inside the hysteresis bands. */
    }

    return operationOn;
}

/** \brief Applies a battery operation change after timer confirmation. */
static void PowerApp_ConfirmBatteryOperation(boolean operationOn)
{
    if (FSM_Timer_IsExpired(FSM_TIMER_POWER_BAT_HYSTERESIS) == TRUE)
    {
        PowerApp_Context.batteryOperationOn = operationOn;
        PowerApp_Context.batteryHysteresisTimerActive = FALSE;
    }
    else
    {
        /* Wait until the battery confirmation timer expires. */
    }
}

/** \brief Starts, retains, or cancels one voltage confirmation timer. */
static void PowerApp_UpdateHysteresisTimer(
    FSM_TimerIdType timerId,
    boolean transitionCondition,
    boolean * const timerActive)
{
    if (timerActive != NULL_PTR)
    {
        if ((transitionCondition == TRUE) && (*timerActive == FALSE))
        {
            FSM_Timer_Start(timerId, POWER_HYSTERESIS_TIME_TICKS);
            *timerActive = TRUE;
        }
        else if (transitionCondition == FALSE)
        {
            FSM_Timer_Stop(timerId);
            *timerActive = FALSE;
        }
        else
        {
            /* The confirmation timer is already running. */
        }
    }
    else
    {
        /* Internal callers always provide a valid timer-state pointer. */
    }
}

/** \brief Restarts timeout monitoring and queues a changed HU power state. */
static Std_ReturnType PowerApp_AcceptHuPowerState(
    Power_HuStateType huPowerState,
    Power_EventType event)
{
    Std_ReturnType result = STD_OK;

    PowerApp_RestartHuTimeout();

    if ((PowerApp_Context.huPowerStateValid == FALSE) ||
        (PowerApp_Context.huPowerState != huPowerState))
    {
        result = Fsm_PostEvent(&PowerApp_Context.fsm,
                               (Fsm_EventType)event);
        if (result == STD_OK)
        {
            PowerApp_Context.huPowerState = huPowerState;
            PowerApp_Context.huPowerStateValid = TRUE;
        }
        else
        {
            /* Retain the previously accepted state until queueing succeeds. */
        }
    }
    else
    {
        /* An unchanged state only refreshes HU timeout monitoring. */
    }

    return result;
}

/** \brief Records HU message loss and queues its power FSM event. */
static Std_ReturnType PowerApp_ReportHuTimeout(void)
{
    Std_ReturnType result;

    PowerApp_Context.huPowerStateValid = FALSE;
    PowerApp_Context.huTimeoutTimerActive = FALSE;
    result = Fsm_PostEvent(&PowerApp_Context.fsm,
                           (Fsm_EventType)POWER_EVENT_HU_TIMEOUT);

    if (result == STD_OK)
    {
        PowerApp_Context.huMessageTimedOut = TRUE;
    }
    else
    {
        PowerApp_Context.huMessageTimedOut = FALSE;
        FSM_Timer_Start(FSM_TIMER_POWER_HU_TIMEOUT,
                        POWER_EVENT_RETRY_TIME_TICKS);
        PowerApp_Context.huTimeoutTimerActive = TRUE;
    }

    return result;
}

/** \brief Queues one state-specific event when the HU timeout expires. */
static void PowerApp_UpdateHuTimeout(void)
{
    if ((PowerApp_Context.huTimeoutTimerActive == TRUE) &&
        (FSM_Timer_IsExpired(FSM_TIMER_POWER_HU_TIMEOUT) == TRUE))
    {
        (void)PowerApp_ReportHuTimeout();
    }
    else
    {
        /* The HU timeout is inactive or has not elapsed. */
    }
}

/** \brief Starts one HU timeout and clears the previous timeout result. */
static void PowerApp_StartHuTimeout(uint32 timeoutTicks)
{
    FSM_Timer_Start(FSM_TIMER_POWER_HU_TIMEOUT, timeoutTicks);
    PowerApp_Context.huMessageTimedOut = FALSE;
    PowerApp_Context.huTimeoutTimerActive = TRUE;
}

/** \brief Restarts the timeout associated with the authoritative FSM state. */
static void PowerApp_RestartHuTimeout(void)
{
    switch ((Power_StateType)Fsm_GetState(&PowerApp_Context.fsm))
    {
        case POWER_STATE_STANDBY:
            PowerApp_StartHuTimeout(POWER_STANDBY_TIMEOUT_TICKS);
            break;

        case POWER_STATE_POWER_ON:
            PowerApp_StartHuTimeout(POWER_ON_TIMEOUT_TICKS);
            break;

        case POWER_STATE_POWER_OFF:
            PowerApp_StartHuTimeout(POWER_OFF_TIMEOUT_TICKS);
            break;

        case POWER_STATE_INIT:
        case POWER_STATE_SLEEP:
        case POWER_STATE_SLEEP_IN:
        case POWER_STATE_ERROR:
            FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
            PowerApp_Context.huMessageTimedOut = FALSE;
            PowerApp_Context.huTimeoutTimerActive = FALSE;
            break;

        default:
            FSM_Timer_Stop(FSM_TIMER_POWER_HU_TIMEOUT);
            PowerApp_Context.huMessageTimedOut = FALSE;
            PowerApp_Context.huTimeoutTimerActive = FALSE;
            break;
    }
}

/** \brief Derives the output request from the synchronized FSM state. */
static void PowerApp_UpdateOutputRequest(void)
{
    switch (PowerApp_Context.state)
    {
        case POWER_STATE_POWER_ON:
            PowerApp_Context.outputEnabled = TRUE;
            break;

        case POWER_STATE_INIT:
        case POWER_STATE_SLEEP:
        case POWER_STATE_SLEEP_IN:
        case POWER_STATE_STANDBY:
        case POWER_STATE_POWER_OFF:
        case POWER_STATE_ERROR:
            PowerApp_Context.outputEnabled = FALSE;
            break;

        default:
            PowerApp_Context.outputEnabled = FALSE;
            break;
    }
}

