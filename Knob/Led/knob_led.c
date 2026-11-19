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
    \file   knob_led.c
    \brief  Implements the event-driven LED application state machine.
            This module coordinates E52248 initialization, asynchronous I2C
            jobs, LED configuration, output control, and fault handling.
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
| 2026-10-15 000.02     DL      Standardized C source documentation
| 2026-10-15 000.03     DL      Added runtime LED setting transitions
| 2026-10-20 000.04     DL      Added VLED and temperature read requests
| 2026-10-20 000.05     DL      Added VDIF and ILED read requests
| 2026-10-30 000.06     DL      Kept LED output off until explicitly enabled
| 2026-10-30 000.07     DL      Removed power actions and toggle control
| 2026-11-05 000.08     DL      Added startup and runtime OTP validation
| 2026-11-05 000.09     DL      Added post-reset communication-ready delay
| 2026-11-08 000.10     DL      Added guarded runtime OTP test-write request
| 2026-11-09 000.11     DL      Used valid address for runtime OTP programming
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/

#include "prj_defs.h"
#include "prj_typedefs.h"

#include "e52248_comm.h"
#include "knob_led_cfg.h"
#include "knob_led_drv.h"
#include "knob_led.h"
#include "knob_led_otp.h"
#include "fsm.h"
#include "fsm_timer.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Delay from reset assertion to I2C initialization, in timer ticks. */
#define LED_RESET_TIME_TICKS    ((uint32)20U)

/** Delay from I2C initialization to address checking, in timer ticks. */
#define LED_I2C_INIT_TIME_TICKS ((uint32)15U)

/** Delay from OTP reset release to the first I2C write, in timer ticks. */
#define LED_OTP_READY_TIME_TICKS ((uint32)10U)

/** Delay from configuration-reset release to the first normal-mode write. */
#define LED_CONFIG_READY_TIME_TICKS ((uint32)15U)

/** Number of consecutive per-channel diagnostic result registers. */
#define LED_DIAG_CHANNEL_COUNT  ((uint8)LED_CHANNEL_COUNT)

/** Number of consecutive Open/Short status registers from address 0xB8. */
#define LED_OPEN_SHORT_STATUS_COUNT ((uint8)4U)

/** Offset used by the E52248 Kelvin-based temperature representation. */
#define LED_KELVIN_OFFSET       ((sint32)273)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* FSM state-entry operations. */
static void LedApp_EntryReset(void);
static void LedApp_EntryI2CInit(void);
static void LedApp_EntryAddrChk(void);
static void LedApp_EntryOtpReset(void);
static void LedApp_EntryOtpReadyWait(void);
static void LedApp_EntryOtpWrite(void);
static void LedApp_EntryOtpValidity(void);
static void LedApp_EntryConfigReset(void);
static void LedApp_EntryConfigReadyWait(void);
static void LedApp_EntrySetCurrent(void);
static void LedApp_EntrySetPort(void);
static void LedApp_EntrySetBrightness(void);
static void LedApp_EntryNormal(void);
static void LedApp_EntryFault(void);

/* FSM state-exit operations. */
static void LedApp_ExitReset(void);
static void LedApp_ExitInit(void);
static void LedApp_ExitNoAction(void);
/* FSM periodic state operations. */
static void LedApp_DoNoAction(void);

/* FSM transition guards. */
static uint8 LedApp_GuardAlways(void);
/* FSM transition actions. */
static void LedApp_ActionNone(void);

/* Module-local support functions. */
static Std_ReturnType LedApp_ContextInit(void);
static void LedApp_FSM_Timer_EventCheck(void);
static void LedApp_CommResultCheck(void);
static Std_ReturnType LedApp_RequestSetting(Led_EventType event);
static Std_ReturnType LedApp_RequestDiagnostic(
    Led_DiagnosticRequestType request,
    uint8 registerAddress,
    E52248_CommSizeType sizeType);
static Std_ReturnType LedApp_CopyDiagnosticRead(void);
#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
static Std_ReturnType LedApp_CopyOpenShortStatus(void);
#endif
static void LedApp_ProcessOtpValidationResult(void);
static void LedApp_UpdateOtpValidationContext(void);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Runtime context owned by the LED application module. */
static Led_ContextType LedApp_Context;

/** Enables cyclic processing after all initialization steps succeed. */
static boolean Led_DriverEnabled = FALSE;

#if (PRJ_LED_OTP_TEST_WRITE_ENABLE == 1U)
/** Prevents repeated OTP programming requests during one power cycle. */
static boolean Led_OtpTestWriteRequested = FALSE;
#endif

/*******************************************************************************
    9. PRIVATE CONSTANTS AND FSM CONFIGURATION
*******************************************************************************/

/** Entry, exit, and periodic operations indexed by Led_StateType. */
static const Fsm_StateOperationType LedApp_StateOperations[LED_STATE_MAX] =
{
    { LedApp_EntryReset,         LedApp_ExitReset,       LedApp_DoNoAction },
    { LedApp_EntryI2CInit,       LedApp_ExitInit,        LedApp_DoNoAction },
    { LedApp_EntryAddrChk,       LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryOtpReset,      LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryOtpReadyWait,  LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryOtpWrite,      LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryOtpValidity,   LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryConfigReset,   LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryConfigReadyWait,
      LedApp_ExitNoAction, LedApp_DoNoAction },
    { LedApp_EntrySetCurrent,    LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntrySetPort,       LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntrySetBrightness, LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryNormal,        LedApp_ExitNoAction,    LedApp_DoNoAction },
    { LedApp_EntryFault,         LedApp_ExitNoAction,    LedApp_DoNoAction }
};

/** State transition table evaluated for each dispatched LED event. */
static const Fsm_TransitionType LedApp_Transitions[] =
{
    { (Fsm_StateType)LED_STATE_RESET, (Fsm_EventType)LED_EVENT_RESET_ON,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_I2C_INIT },
    { (Fsm_StateType)LED_STATE_I2C_INIT, (Fsm_EventType)LED_EVENT_I2C_ON,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_ADDR_CHK },
    { (Fsm_StateType)LED_STATE_ADDR_CHK, (Fsm_EventType)LED_EVENT_ADDR_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_SET_CURRENT },
    { (Fsm_StateType)LED_STATE_ADDR_CHK, (Fsm_EventType)LED_EVENT_COMM_ERROR,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_OTP_RESET },
    { (Fsm_StateType)LED_STATE_OTP_RESET, (Fsm_EventType)LED_EVENT_RESET_DONE,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_OTP_READY_WAIT },
    { (Fsm_StateType)LED_STATE_OTP_READY_WAIT,
      (Fsm_EventType)LED_EVENT_OTP_READY,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_OTP_WRITE },
    { (Fsm_StateType)LED_STATE_OTP_WRITE, (Fsm_EventType)LED_EVENT_WRITE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_OTP_VALIDITY },
    { (Fsm_StateType)LED_STATE_OTP_WRITE, (Fsm_EventType)LED_EVENT_COMM_ERROR,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_FAULT },
    { (Fsm_StateType)LED_STATE_OTP_VALIDITY, (Fsm_EventType)LED_EVENT_WRITE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_CONFIG_RESET },
    { (Fsm_StateType)LED_STATE_OTP_VALIDITY,
      (Fsm_EventType)LED_EVENT_UPDATE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_NORMAL },
    { (Fsm_StateType)LED_STATE_OTP_VALIDITY,
      (Fsm_EventType)LED_EVENT_OTP_INVALID,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_FAULT },
    { (Fsm_StateType)LED_STATE_OTP_VALIDITY,
      (Fsm_EventType)LED_EVENT_COMM_ERROR,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_FAULT },
    { (Fsm_StateType)LED_STATE_CONFIG_RESET,
      (Fsm_EventType)LED_EVENT_RESET_DONE,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_CONFIG_READY_WAIT },
    { (Fsm_StateType)LED_STATE_CONFIG_READY_WAIT,
      (Fsm_EventType)LED_EVENT_CONFIG_READY,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_SET_CURRENT },
    { (Fsm_StateType)LED_STATE_SET_CURRENT, (Fsm_EventType)LED_EVENT_WRITE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_SET_BRIGHT },
    { (Fsm_StateType)LED_STATE_SET_CURRENT, (Fsm_EventType)LED_EVENT_UPDATE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_NORMAL },
    { (Fsm_StateType)LED_STATE_SET_CURRENT,
      (Fsm_EventType)LED_EVENT_COMM_ERROR,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_FAULT },
    { (Fsm_StateType)LED_STATE_SET_BRIGHT, (Fsm_EventType)LED_EVENT_WRITE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_SET_PORT },
    { (Fsm_StateType)LED_STATE_SET_BRIGHT, (Fsm_EventType)LED_EVENT_UPDATE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_NORMAL },
    { (Fsm_StateType)LED_STATE_SET_BRIGHT,
      (Fsm_EventType)LED_EVENT_COMM_ERROR,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_FAULT },
    { (Fsm_StateType)LED_STATE_SET_PORT, (Fsm_EventType)LED_EVENT_WRITE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_NORMAL },
    { (Fsm_StateType)LED_STATE_SET_PORT, (Fsm_EventType)LED_EVENT_UPDATE_OK,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_NORMAL },
    { (Fsm_StateType)LED_STATE_SET_PORT, (Fsm_EventType)LED_EVENT_COMM_ERROR,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_FAULT },
    { (Fsm_StateType)LED_STATE_NORMAL, (Fsm_EventType)LED_EVENT_CUR_REQ,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_SET_CURRENT },
    { (Fsm_StateType)LED_STATE_NORMAL, (Fsm_EventType)LED_EVENT_PORT_REQ,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_SET_PORT },
    { (Fsm_StateType)LED_STATE_NORMAL, (Fsm_EventType)LED_EVENT_BRIGHT_REQ,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_SET_BRIGHT },
    { (Fsm_StateType)LED_STATE_NORMAL,
      (Fsm_EventType)LED_EVENT_OTP_VALID_REQ,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_OTP_VALIDITY },
#if (PRJ_LED_OTP_TEST_WRITE_ENABLE == 1U)
    { (Fsm_StateType)LED_STATE_NORMAL,
      (Fsm_EventType)LED_EVENT_OTP_WRITE_REQ,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_OTP_RESET },
#endif
    { (Fsm_StateType)LED_STATE_NORMAL, (Fsm_EventType)LED_EVENT_FAULT,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_FAULT },
    { (Fsm_StateType)LED_STATE_NORMAL, (Fsm_EventType)LED_EVENT_RESET,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_RESET },
    { (Fsm_StateType)LED_STATE_FAULT, (Fsm_EventType)LED_EVENT_RESET,
      LedApp_GuardAlways, LedApp_ActionNone,
      (Fsm_StateType)LED_STATE_RESET }
};

#define LED_TRANSITION_COUNT                                      \
    ((uint16)(sizeof(LedApp_Transitions) /                        \
              sizeof(LedApp_Transitions[0])))

/** Generic FSM configuration used to initialize the LED state machine. */
static const Fsm_ConfigType LedApp_Config =
{
    (Fsm_StateType)LED_STATE_RESET,
    (Fsm_StateType)LED_STATE_MAX,
    (Fsm_EventType)LED_EVENT_MAX,
    LedApp_StateOperations,
    LedApp_Transitions,
    LED_TRANSITION_COUNT
};

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes the LED hardware abstraction and application FSM. */
void LedApp_Init(void)
{
    Std_ReturnType contextInitResult;
    Std_ReturnType commInitResult;
    Std_ReturnType ledFsmInitResult;

    contextInitResult = LedApp_ContextInit();

#if (PRJ_LED_OTP_TEST_WRITE_ENABLE == 1U)
    Led_OtpTestWriteRequested = FALSE;
#endif

    GpioSet_E52248_OtpProg(STD_OFF);
    GpioSet_E52248_OutputEnable(STD_OFF);
    GpioSet_E52248_VccEnable(STD_ON);
    GpioSet_E52248_nReset(STD_OFF);
    commInitResult = E52248_Comm_Init();
    ledFsmInitResult = Fsm_Init(&LedApp_Context.fsm, &LedApp_Config);
    if ((contextInitResult == STD_OK) &&
        (commInitResult == STD_OK) &&
        (ledFsmInitResult == STD_OK))
    {
        Led_DriverEnabled = TRUE;
    }
    else
    {
        Led_DriverEnabled = FALSE;
    }

    LedApp_Context.state = LED_STATE_RESET;
    LedApp_Context.event = LED_EVENT_MAX;
}

/** \brief Advances E52248 communication and consumes completed job results. */
void LedApp_MainFunction5ms(void)
{
    if (Led_DriverEnabled == TRUE)
    {
        E52248_Comm_MainFunction5ms();
        LedApp_CommResultCheck();
    }
}

/** \brief Dispatches one LED event and executes the active state operation. */
void LedApp_MainFunction10ms(void)
{
  if (Led_DriverEnabled == TRUE)
  {
    LedApp_FSM_Timer_EventCheck();

    Fsm_Dispatch(&LedApp_Context.fsm);
    Fsm_Do(&LedApp_Context.fsm);
    LedApp_Context.state =
      (Led_StateType)Fsm_GetState(&LedApp_Context.fsm);
    LedApp_Context.event =
      (Led_EventType)Fsm_GetLastEvent(&LedApp_Context.fsm);
  }
}

/**
 * \brief Enables output after a valid LCAN brightness setting completes.
 *
 * \return STD_OK when enabled in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_EnableOutput(void)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((Led_DriverEnabled == TRUE) &&
        (Fsm_GetState(&LedApp_Context.fsm) ==
            (Fsm_StateType)LED_STATE_NORMAL) &&
        (LedApp_Context.settingRequestPending == FALSE))
    {
        LedApp_Context.outputEnabled = TRUE;
        GpioSet_E52248_OutputEnable(STD_ON);
        result = STD_OK;
    }
    else
    {
        /* Retry after LED initialization or the active setting write. */
    }

    return result;
}

/** \brief Clears the latched enable state and disables the E52248 output. */
void LedApp_DisableOutput(void)
{
    LedApp_Context.outputEnabled = FALSE;
    GpioSet_E52248_OutputEnable(STD_OFF);
}

/** \brief Reports whether the E52248 output is currently enabled. */
boolean LedApp_IsOutputEnabled(void)
{
    return LedApp_Context.outputEnabled;
}

/**
 * \brief Requests a runtime update of the configured current values.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestSetCurrent(uint8 portNumber, uint16 currentValue)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 previousValues[LED_CHANNEL_COUNT];
    uint8 index;

    if ((portNumber <= LED_PORT_MAX) &&
        (currentValue <= LED_CURRENT_VALUE_MAX))
    {
        for (index = (uint8)0U;
             index < (uint8)LED_CHANNEL_COUNT;
             index++)
        {
            previousValues[index] = LedApp_Context.currentValues[index];
        }

        if (portNumber == LED_PORT_MAX)
        {
            for (index = (uint8)0U;
                 index < (uint8)LED_CHANNEL_COUNT;
                 index++)
            {
                LedApp_Context.currentValues[index] = currentValue;
            }
        }
        else
        {
            LedApp_Context.currentValues[portNumber] = currentValue;
        }

        result = LedApp_RequestSetting(LED_EVENT_CUR_REQ);
        if (result != STD_OK)
        {
            for (index = (uint8)0U;
                 index < (uint8)LED_CHANNEL_COUNT;
                 index++)
            {
                LedApp_Context.currentValues[index] = previousValues[index];
            }
        }
        else
        {
            /* The accepted values are consumed on entry to SET_CURRENT. */
        }
    }
    else
    {
        /* The port selector or current value is outside its valid range. */
    }

    return result;
}

/**
 * \brief Requests a runtime update of the configured port values.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestSetPort(void)
{
    return LedApp_RequestSetting(LED_EVENT_PORT_REQ);
}

/**
 * \brief Requests a runtime brightness update using a CLU brightness step.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestSetBrightness(uint8 brightnessStep)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 requestedValue = (uint16)0U;
    uint16 previousValue;

    if (LedCfg_GetBrightnessValue(brightnessStep,
                                  &requestedValue) == STD_OK)
    {
        previousValue = LedApp_Context.brightnessValue;
        LedApp_Context.brightnessValue = requestedValue;

        result = LedApp_RequestSetting(LED_EVENT_BRIGHT_REQ);
        if (result != STD_OK)
        {
            LedApp_Context.brightnessValue = previousValue;
        }
        else
        {
            /* The accepted value is consumed on entry to SET_BRIGHT. */
        }
    }
    else
    {
        /* Values 0, 1, and values above 200 are ignored. */
    }

    return result;
}

/**
 * \brief Requests validation of every configured OTP address and value.
 *
 * \return STD_OK when the request is queued; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestOtpValidation(void)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((Led_DriverEnabled == TRUE) &&
        (Fsm_GetState(&LedApp_Context.fsm) ==
            (Fsm_StateType)LED_STATE_NORMAL) &&
        (LedApp_Context.settingRequestPending == FALSE) &&
        (LedApp_Context.diagnosticRequest == LED_DIAG_REQUEST_NONE))
    {
        result = Fsm_PostEvent(
            &LedApp_Context.fsm,
            (Fsm_EventType)LED_EVENT_OTP_VALID_REQ);

        if (result == STD_OK)
        {
            LedApp_Context.updateMode = LED_UPDATE_MODE_RUNTIME;
            LedApp_Context.settingRequestPending = TRUE;
            LedApp_Context.otpValidationValid = FALSE;
        }
        else
        {
            /* The event queue did not accept the validation request. */
        }
    }
    else
    {
        /* Validation is accepted only while NORMAL is otherwise idle. */
    }

    return result;
}

#if (PRJ_LED_OTP_TEST_WRITE_ENABLE == 1U)
/** \brief Queues one guarded OTP programming request from NORMAL. */
Std_ReturnType LedApp_RequestOtpWrite(void)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((Led_DriverEnabled == TRUE) &&
        (Led_OtpTestWriteRequested == FALSE) &&
        (Fsm_GetState(&LedApp_Context.fsm) ==
            (Fsm_StateType)LED_STATE_NORMAL) &&
        (LedApp_Context.settingRequestPending == FALSE) &&
        (LedApp_Context.diagnosticRequest == LED_DIAG_REQUEST_NONE))
    {
        result = Fsm_PostEvent(
            &LedApp_Context.fsm,
            (Fsm_EventType)LED_EVENT_OTP_WRITE_REQ);

        if (result == STD_OK)
        {
            LedApp_Context.settingRequestPending = TRUE;
            Led_OtpTestWriteRequested = TRUE;
        }
        else
        {
            /* The event queue did not accept the OTP write request. */
        }
    }
    else
    {
        /* OTP writing requires an idle NORMAL state and a fresh request. */
    }

    return result;
}
#endif

/** \brief Requests an asynchronous read of VLED result channels 0 through 15. */
Std_ReturnType LedApp_RequestReadVled(void)
{
    return LedApp_RequestDiagnostic(LED_DIAG_REQUEST_VLED,
                                    REG_RESULT_VLED_0,
                                    E_SIZE_TYPE_16);
}

/** \brief Requests an asynchronous read of VDIF result channels 0 through 15. */
Std_ReturnType LedApp_RequestReadVdif(void)
{
    return LedApp_RequestDiagnostic(LED_DIAG_REQUEST_VDIF,
                                    REG_RESULT_VDIF_0,
                                    E_SIZE_TYPE_16);
}

/** \brief Requests an asynchronous read of ILED result channels 0 through 15. */
Std_ReturnType LedApp_RequestReadIled(void)
{
    return LedApp_RequestDiagnostic(LED_DIAG_REQUEST_ILED,
                                    REG_RESULT_ILED_0,
                                    E_SIZE_TYPE_16);
}

/** \brief Requests an asynchronous read of the junction-temperature result. */
Std_ReturnType LedApp_RequestReadTemperature(void)
{
    return LedApp_RequestDiagnostic(LED_DIAG_REQUEST_TEMPERATURE,
                                    REG_RESULT_VT,
                                    E_SIZE_TYPE_1);
}

#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
/** \brief Requests E52248 Open and Short status words from 0xB8 to 0xBB. */
Std_ReturnType LedApp_RequestReadOpenShort(void)
{
    return LedApp_RequestDiagnostic(LED_DIAG_REQUEST_OPEN_SHORT,
                                    REG_LED_OPEN_0_7,
                                    E_SIZE_TYPE_4);
}
#endif

/**
 * \brief Queues a request to enter the LED fault state.
 *
 * \return STD_OK when queued successfully; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestFault(void)
{
    return Fsm_PostEvent(&LedApp_Context.fsm,
                         (Fsm_EventType)LED_EVENT_FAULT);
}

/**
 * \brief Queues a request to restart the LED initialization sequence.
 *
 * \return STD_OK when queued successfully; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestReset(void)
{
    return Fsm_PostEvent(&LedApp_Context.fsm,
                         (Fsm_EventType)LED_EVENT_RESET);
}

/**
 * \brief Returns the last state captured by the 10 ms main function.
 *
 * \return Current observable LED application state.
 */
Led_StateType LedApp_GetState(void)
{
    return LedApp_Context.state;
}

/**
 * \brief Returns a read-only diagnostic view of the LED context.
 *
 * \return Pointer to the statically allocated LED application context.
 */
const Led_ContextType * LedApp_GetContext(void)
{
    return &LedApp_Context;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Initializes the LED context and loads configuration defaults. */
static Std_ReturnType LedApp_ContextInit(void)
{
    Std_ReturnType result;
    uint8 index;

    LedApp_Context.ctrl_ic = E52248_Comm_GetContext();
    LedApp_Context.updateMode = LED_UPDATE_MODE_INIT;
    LedApp_Context.settingRequestPending = FALSE;
    LedApp_Context.outputEnabled = FALSE;
    LedApp_Context.otpSlaveAddress = I2C_ADDR_INIT;
    LedApp_Context.otpProgrammingActive = FALSE;
    LedApp_Context.otpValidationValid = FALSE;
    LedApp_Context.otpValidationIndex = (uint8)0U;
    LedApp_Context.otpValidationExpectedValue = (uint16)0U;
    LedApp_Context.otpValidationActualValue = (uint16)0U;
    LedApp_Context.brightnessValue = LedCfg_GetDefaultBrightnessValue();
    LedApp_Context.diagnosticRequest = LED_DIAG_REQUEST_NONE;
    LedApp_Context.vledValuesValid = FALSE;
    LedApp_Context.vdifValuesValid = FALSE;
    LedApp_Context.iledValuesValid = FALSE;
    LedApp_Context.temperatureRaw = (uint16)0U;
    LedApp_Context.temperatureCelsius = (sint16)0;
    LedApp_Context.temperatureValid = FALSE;

#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
    LedApp_Context.openStatus0To7 = (uint16)0U;
    LedApp_Context.openStatus8To15 = (uint16)0U;
    LedApp_Context.shortStatus0To7 = (uint16)0U;
    LedApp_Context.shortStatus8To15 = (uint16)0U;
    LedApp_Context.openShortStatusValid = FALSE;
#endif

    for (index = (uint8)0U;
         index < LED_DIAG_CHANNEL_COUNT;
         index++)
    {
        LedApp_Context.vledValues[index] = (uint16)0U;
        LedApp_Context.vdifValues[index] = (uint16)0U;
        LedApp_Context.iledValues[index] = (uint16)0U;
    }

    result = LedCfg_LoadDefaultCurrent(LedApp_Context.currentValues);

    return result;
}

/** \brief Converts expired LED timers into queued FSM events. */
static void LedApp_FSM_Timer_EventCheck(void)
{
    Led_StateType state; 
    if (FSM_Timer_IsExpired(FSM_TIMER_LED_RESET) == TRUE)
    {
        state = LedApp_GetState();
        switch (state)
        {
            case LED_STATE_RESET : 
                (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                    (Fsm_EventType)LED_EVENT_RESET_ON);
                break;
            case LED_STATE_I2C_INIT : 
                (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                    (Fsm_EventType)LED_EVENT_I2C_ON);
                break;
            case LED_STATE_OTP_RESET:
            case LED_STATE_CONFIG_RESET:
                (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                    (Fsm_EventType)LED_EVENT_RESET_DONE);
                break;
            case LED_STATE_CONFIG_READY_WAIT:
                (void)Fsm_PostEvent(
                    &LedApp_Context.fsm,
                    (Fsm_EventType)LED_EVENT_CONFIG_READY);
                break;
            case LED_STATE_OTP_READY_WAIT:
                (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                    (Fsm_EventType)LED_EVENT_OTP_READY);
                break;
            default : 
                break; 
        }
    }
}

/** \brief Converts completed E52248 jobs into queued LED FSM events. */
static void LedApp_CommResultCheck(void)
{
    E52248_JobResultType result;
    Fsm_StateType state = Fsm_GetState(&LedApp_Context.fsm);
    Led_EventType completionEvent;
    LedOtp_SequenceResultType otpResult;

    if (E52248_Comm_TakeResult(&result) == STD_OK)
    {
        switch (result)
        {
            case E52248_JOB_RESULT_READ_OK:
                if (state == (Fsm_StateType)LED_STATE_ADDR_CHK)
                {
                    (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                        (Fsm_EventType)LED_EVENT_ADDR_OK);
                }
                else if (state ==
                         (Fsm_StateType)LED_STATE_OTP_VALIDITY)
                {
                    LedApp_ProcessOtpValidationResult();
                }
                else if ((state == (Fsm_StateType)LED_STATE_NORMAL) &&
                         (LedApp_Context.diagnosticRequest !=
                             LED_DIAG_REQUEST_NONE))
                {
                    if (LedApp_CopyDiagnosticRead() != STD_OK)
                    {
                        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                            (Fsm_EventType)LED_EVENT_FAULT);
                    }
                    else
                    {
                        /* The completed diagnostic result is now observable. */
                    }
                }
                else
                {
                    /* Ignore reads that are not owned by an active request. */
                }
                break;

            case E52248_JOB_RESULT_WRITE_OK:
                if ((state == (Fsm_StateType)LED_STATE_SET_CURRENT) ||
                    (state == (Fsm_StateType)LED_STATE_SET_PORT) ||
                    (state == (Fsm_StateType)LED_STATE_SET_BRIGHT))
                {
                    if (LedApp_Context.updateMode == LED_UPDATE_MODE_RUNTIME)
                    {
                        completionEvent = LED_EVENT_UPDATE_OK;
                    }
                    else
                    {
                        completionEvent = LED_EVENT_WRITE_OK;
                    }

                    (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                        (Fsm_EventType)completionEvent);
                }
                else if (state == (Fsm_StateType)LED_STATE_OTP_WRITE)
                {
                    otpResult = LedOtp_ContinueWrite();
                    if (otpResult == LED_OTP_SEQUENCE_COMPLETE)
                    {
                        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                            (Fsm_EventType)LED_EVENT_WRITE_OK);
                    }
                    else if (otpResult == LED_OTP_SEQUENCE_ERROR)
                    {
                        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
                    }
                    else
                    {
                        /* The next OTP register transaction is pending. */
                    }
                }
                else if (state == (Fsm_StateType)LED_STATE_OTP_VALIDITY)
                {
                    LedApp_ProcessOtpValidationResult();
                }
                else
                {
                    /* Ignore write results outside the configuration states. */
                }
                break;

            case E52248_JOB_RESULT_ERROR_COM:
            case E52248_JOB_RESULT_ERROR_CRC:
                if ((state == (Fsm_StateType)LED_STATE_NORMAL) &&
                    (LedApp_Context.diagnosticRequest !=
                        LED_DIAG_REQUEST_NONE))
                {
                    LedApp_Context.diagnosticRequest =
                        LED_DIAG_REQUEST_NONE;
                    (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                        (Fsm_EventType)LED_EVENT_FAULT);
                }
                else
                {
                    if (state == (Fsm_StateType)LED_STATE_OTP_WRITE)
                    {
                        LedOtp_AbortWrite();
                    }
                    else if (state ==
                             (Fsm_StateType)LED_STATE_OTP_VALIDITY)
                    {
                        LedOtp_AbortValidityCheck();
                        LedApp_UpdateOtpValidationContext();
                    }
                    else
                    {
                        /* No OTP write sequence is active. */
                    }

                    (void)Fsm_PostEvent(&LedApp_Context.fsm,
                                        (Fsm_EventType)LED_EVENT_COMM_ERROR);
                }
                break;

            case E52248_JOB_RESULT_NONE:
            case E52248_JOB_RESULT_PENDING:
            default:
                break;
        }
    }
    else
    {
        /* No completed communication result is available. */
    }
}

/** \brief Advances OTP validation and reports its FSM completion event. */
static void LedApp_ProcessOtpValidationResult(void)
{
    LedOtp_SequenceResultType validationResult;
    Led_EventType completionEvent;

    validationResult = LedOtp_ContinueValidityCheck();
    LedApp_UpdateOtpValidationContext();

    if (validationResult == LED_OTP_SEQUENCE_COMPLETE)
    {
        if (LedApp_Context.otpProgrammingActive == TRUE)
        {
            completionEvent = LED_EVENT_WRITE_OK;
        }
        else if (LedApp_Context.updateMode == LED_UPDATE_MODE_RUNTIME)
        {
            completionEvent = LED_EVENT_UPDATE_OK;
        }
        else
        {
            completionEvent = LED_EVENT_WRITE_OK;
        }

        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)completionEvent);
    }
    else if (validationResult == LED_OTP_SEQUENCE_INVALID)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_OTP_INVALID);
    }
    else if (validationResult == LED_OTP_SEQUENCE_ERROR)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* The next page-select or OTP data-read job is pending. */
    }
}

/** \brief Copies the latest OTP comparison into the observable LED context. */
static void LedApp_UpdateOtpValidationContext(void)
{
    const LedOtp_ValidationResultType *validationResult;

    validationResult = LedOtp_GetValidationResult();
    if (validationResult != NULL_PTR)
    {
        LedApp_Context.otpValidationValid = validationResult->valid;
        LedApp_Context.otpValidationIndex = validationResult->configIndex;
        LedApp_Context.otpValidationExpectedValue =
            validationResult->expectedValue;
        LedApp_Context.otpValidationActualValue =
            validationResult->actualValue;
    }
    else
    {
        LedApp_Context.otpValidationValid = FALSE;
    }
}

/** \brief Asserts E52248 reset and starts the reset delay timer. */
static void LedApp_EntryReset(void)
{
    LedApp_Context.updateMode = LED_UPDATE_MODE_INIT;
    LedApp_Context.settingRequestPending = FALSE;
    LedApp_Context.diagnosticRequest = LED_DIAG_REQUEST_NONE;
    LedApp_Context.vledValuesValid = FALSE;
    LedApp_Context.vdifValuesValid = FALSE;
    LedApp_Context.iledValuesValid = FALSE;
    LedApp_Context.temperatureValid = FALSE;

    GpioSet_E52248_nReset(STD_ON);
    FSM_Timer_Start(FSM_TIMER_LED_RESET,
                    LED_RESET_TIME_TICKS);
}

/** \brief Initializes I2C and starts the initialization delay timer. */
static void LedApp_EntryI2CInit(void)
{
    I2C_E52248_Init();
    FSM_Timer_Start(FSM_TIMER_LED_RESET,
                    LED_I2C_INIT_TIME_TICKS);
}

/** \brief Starts an asynchronous E52248 address-check read. */
static void LedApp_EntryAddrChk(void)
{
    /* Check whether the configured E52248 address responds correctly. */
    if (E52248_Comm_StartRead(I2C_ADDR_VALID,
                              REG_EVENT_STATUS,
                              E_SIZE_TYPE_4) != STD_OK)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* Completion is reported asynchronously. */
    }
}

/** \brief Asserts reset and starts the delay required before OTP writing. */
static void LedApp_EntryOtpReset(void)
{
    if (LedApp_Context.updateMode == LED_UPDATE_MODE_RUNTIME)
    {
        LedApp_Context.otpSlaveAddress = I2C_ADDR_VALID;
    }
    else
    {
        LedApp_Context.otpSlaveAddress = I2C_ADDR_INIT;
    }

    LedApp_Context.otpProgrammingActive = TRUE;
    LedApp_Context.updateMode = LED_UPDATE_MODE_INIT;
    LedApp_Context.settingRequestPending = TRUE;
    LedApp_Context.outputEnabled = FALSE;
    LedApp_Context.otpValidationValid = FALSE;

    GpioSet_E52248_OutputEnable(STD_OFF);
    GpioSet_E52248_OtpProg(STD_ON);
    GpioSet_E52248_nReset(STD_ON);
    FSM_Timer_Start(FSM_TIMER_LED_RESET,
                    LED_RESET_TIME_TICKS);
}

/** \brief Releases reset and waits for OTP programming-mode readiness. */
static void LedApp_EntryOtpReadyWait(void)
{
    GpioSet_E52248_OtpProg(STD_ON);
    GpioSet_E52248_nReset(STD_OFF);
    FSM_Timer_Start(FSM_TIMER_LED_RESET,
                    LED_OTP_READY_TIME_TICKS);
}

/** \brief Reserved entry operation for the E52248 OTP write sequence. */
static void LedApp_EntryOtpWrite(void)
{
    if (LedOtp_StartWrite(LedApp_Context.otpSlaveAddress) != STD_OK)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* Completion is reported asynchronously. */
    }
}

/** \brief Reserved entry operation for E52248 OTP validity checking. */
static void LedApp_EntryOtpValidity(void)
{
    uint8 slaveAddress;

    LedApp_Context.otpValidationValid = FALSE;

    if (LedApp_Context.otpProgrammingActive == TRUE)
    {
        slaveAddress = LedApp_Context.otpSlaveAddress;
    }
    else if (LedApp_Context.updateMode == LED_UPDATE_MODE_RUNTIME)
    {
        slaveAddress = I2C_ADDR_VALID;
    }
    else
    {
        slaveAddress = I2C_ADDR_INIT;
    }

    if (LedOtp_StartValidityCheck(slaveAddress) != STD_OK)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* Page selection, reads, and comparisons continue asynchronously. */
    }

    LedApp_UpdateOtpValidationContext();
}

/** \brief Asserts reset and starts the delay before normal configuration. */
static void LedApp_EntryConfigReset(void)
{
    GpioSet_E52248_nReset(STD_ON);
    GpioSet_E52248_OtpProg(STD_OFF);
    FSM_Timer_Start(FSM_TIMER_LED_RESET,
                    LED_RESET_TIME_TICKS);
}

/** \brief Releases reset and waits until normal communication is ready. */
static void LedApp_EntryConfigReadyWait(void)
{
    GpioSet_E52248_OtpProg(STD_OFF);
    GpioSet_E52248_nReset(STD_OFF);
    FSM_Timer_Start(FSM_TIMER_LED_RESET,
                    LED_CONFIG_READY_TIME_TICKS);
}

/** \brief Starts the asynchronous 16-channel current configuration write. */
static void LedApp_EntrySetCurrent(void)
{
    if (E52248_Comm_StartWrite(I2C_ADDR_VALID,
                               REG_IMM_BUS_CURRENT_0,
                               E_SIZE_TYPE_16,
                               LedApp_Context.currentValues) != STD_OK)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* Completion is reported asynchronously. */
    }
}

/** \brief Starts the asynchronous LED port-enable configuration write. */
static void LedApp_EntrySetPort(void)
{
    uint16 data_buf[4];

    data_buf[0] = (uint16)0xFCU;
    data_buf[1] = (uint16)0x03U;
    data_buf[2] = (uint16)0x00U;
    data_buf[3] = (uint16)0x00U;

    if (E52248_Comm_StartWrite(I2C_ADDR_VALID, 
                                REG_IMM_ENABLE_0_7, 
                                E_SIZE_TYPE_4,
                                data_buf) != STD_OK)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* Completion is reported asynchronously. */
    }
}

#if 1
/** \brief Starts the asynchronous 16-channel brightness configuration write. */
static void LedApp_EntrySetBrightness(void)
{
    uint16 data_buf[LED_CHANNEL_COUNT];
    uint8 i;

    data_buf[0] = (uint16)0U;
    data_buf[1] = (uint16)0U;

    for (i = (uint8)2U; i < (uint8)LED_CHANNEL_COUNT; i++)
    {
        data_buf[i] = LedApp_Context.brightnessValue;
    }

    if (E52248_Comm_StartWrite(I2C_ADDR_VALID,
                               REG_IMM_BUS_PULSE_0,
                               E_SIZE_TYPE_16,
                               data_buf) != STD_OK)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* Completion is reported asynchronously. */
    }
}
#else
/** \brief Starts one atomic brightness write for every enabled LED channel. */
static void LedApp_EntrySetBrightness(void)
{
    uint16 brightnessValue = LedApp_Context.brightnessValue;

    if (E52248_Comm_StartWrite(I2C_ADDR_VALID,
                               REG_IMM_BUS_PULSE_ALL,
                               E_SIZE_TYPE_1,
                               &brightnessValue) != STD_OK)
    {
        (void)Fsm_PostEvent(&LedApp_Context.fsm,
                            (Fsm_EventType)LED_EVENT_COMM_ERROR);
    }
    else
    {
        /* Completion is reported asynchronously. */
    }
}
#endif

/** \brief Restores the latched output state after configuration completes. */
static void LedApp_EntryNormal(void)
{
    LedApp_Context.updateMode = LED_UPDATE_MODE_RUNTIME;
    LedApp_Context.settingRequestPending = FALSE;
    LedApp_Context.otpProgrammingActive = FALSE;

    if (LedApp_Context.outputEnabled == TRUE)
    {
        GpioSet_E52248_OutputEnable(STD_ON);
    }
    else
    {
        GpioSet_E52248_OutputEnable(STD_OFF);
    }

}

/** \brief Disables LED output when the application enters the fault state. */
static void LedApp_EntryFault(void)
{
    LedApp_Context.settingRequestPending = FALSE;
    LedApp_Context.otpProgrammingActive = FALSE;
    LedApp_Context.diagnosticRequest = LED_DIAG_REQUEST_NONE;
    LedApp_Context.outputEnabled = FALSE;

    GpioSet_E52248_OutputEnable(STD_OFF);
}

/** \brief Releases the E52248 reset signal when leaving the reset state. */
static void LedApp_ExitReset(void)
{
    GpioSet_E52248_nReset(STD_OFF);
}

/** \brief Provides the I2C initialization-state exit operation. */
static void LedApp_ExitInit(void)
{
    /* No exit processing is required. */
}

/** \brief Provides a reusable state-exit operation with no behavior. */
static void LedApp_ExitNoAction(void)
{
    /* No common LED exit processing is required. */
}

/** \brief Provides a reusable periodic state operation with no behavior. */
static void LedApp_DoNoAction(void)
{
    /* The associated states require no periodic processing. */
}

/**
 * \brief Unconditionally permits a configured FSM transition.
 *
 * \return STD_TRUE for every invocation.
 */
static uint8 LedApp_GuardAlways(void)
{
    return STD_TRUE;
}

/** \brief Provides a reusable transition action with no behavior. */
static void LedApp_ActionNone(void)
{
    /* State entry functions apply the requested LED output. */
}

/**
 * \brief Queues one runtime setting request while the LED FSM is normal.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
static Std_ReturnType LedApp_RequestSetting(Led_EventType event)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((Led_DriverEnabled == TRUE) &&
        (Fsm_GetState(&LedApp_Context.fsm) ==
            (Fsm_StateType)LED_STATE_NORMAL) &&
        (LedApp_Context.settingRequestPending == FALSE) &&
        (LedApp_Context.diagnosticRequest == LED_DIAG_REQUEST_NONE))
    {
        result = Fsm_PostEvent(&LedApp_Context.fsm, (Fsm_EventType)event);
        if (result == STD_OK)
        {
            LedApp_Context.settingRequestPending = TRUE;
        }
        else
        {
            /* The event queue could not accept the setting request. */
        }
    }
    else
    {
        /* Runtime setting requests are accepted only from the normal state. */
    }

    return result;
}

/** \brief Starts one diagnostic read while no LED transaction is pending. */
static Std_ReturnType LedApp_RequestDiagnostic(
    Led_DiagnosticRequestType request,
    uint8 registerAddress,
    E52248_CommSizeType sizeType)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((Led_DriverEnabled == TRUE) &&
        (Fsm_GetState(&LedApp_Context.fsm) ==
            (Fsm_StateType)LED_STATE_NORMAL) &&
        (LedApp_Context.settingRequestPending == FALSE) &&
        (LedApp_Context.diagnosticRequest == LED_DIAG_REQUEST_NONE) &&
        (request > LED_DIAG_REQUEST_NONE) &&
        (request < LED_DIAG_REQUEST_MAX))
    {
        result = E52248_Comm_StartRead(I2C_ADDR_VALID,
                                       registerAddress,
                                       sizeType);
        if (result == STD_OK)
        {
            LedApp_Context.diagnosticRequest = request;

            if (request == LED_DIAG_REQUEST_VLED)
            {
                LedApp_Context.vledValuesValid = FALSE;
            }
            else if (request == LED_DIAG_REQUEST_VDIF)
            {
                LedApp_Context.vdifValuesValid = FALSE;
            }
            else if (request == LED_DIAG_REQUEST_ILED)
            {
                LedApp_Context.iledValuesValid = FALSE;
            }
            else if (request == LED_DIAG_REQUEST_TEMPERATURE)
            {
                LedApp_Context.temperatureValid = FALSE;
            }
#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
            else if (request == LED_DIAG_REQUEST_OPEN_SHORT)
            {
                LedApp_Context.openShortStatusValid = FALSE;
            }
#endif
            else
            {
                /* The request range was validated before communication. */
            }
        }
        else
        {
            /* The communication service is busy or rejected the request. */
        }
    }
    else
    {
        /* Diagnostic reads are accepted only while NORMAL and idle. */
    }

    return result;
}

/** \brief Copies and converts the completed diagnostic read result. */
static Std_ReturnType LedApp_CopyDiagnosticRead(void)
{
    Std_ReturnType result = STD_NOT_OK;

    switch (LedApp_Context.diagnosticRequest)
    {
        case LED_DIAG_REQUEST_VLED:
            result = E52248_Comm_CopyReadData(
                LedApp_Context.vledValues,
                LED_DIAG_CHANNEL_COUNT);
            if (result == STD_OK)
            {
                LedApp_Context.vledValuesValid = TRUE;
            }
            else
            {
                LedApp_Context.vledValuesValid = FALSE;
            }
            break;

        case LED_DIAG_REQUEST_VDIF:
            result = E52248_Comm_CopyReadData(
                LedApp_Context.vdifValues,
                LED_DIAG_CHANNEL_COUNT);
            if (result == STD_OK)
            {
                LedApp_Context.vdifValuesValid = TRUE;
            }
            else
            {
                LedApp_Context.vdifValuesValid = FALSE;
            }
            break;

        case LED_DIAG_REQUEST_ILED:
            result = E52248_Comm_CopyReadData(
                LedApp_Context.iledValues,
                LED_DIAG_CHANNEL_COUNT);
            if (result == STD_OK)
            {
                LedApp_Context.iledValuesValid = TRUE;
            }
            else
            {
                LedApp_Context.iledValuesValid = FALSE;
            }
            break;

        case LED_DIAG_REQUEST_TEMPERATURE:
            result = E52248_Comm_CopyReadData(
                &LedApp_Context.temperatureRaw,
                (uint8)1U);
            if (result == STD_OK)
            {
                LedApp_Context.temperatureCelsius =
                    (sint16)((sint32)LedApp_Context.temperatureRaw -
                             LED_KELVIN_OFFSET);
                LedApp_Context.temperatureValid = TRUE;
            }
            else
            {
                LedApp_Context.temperatureValid = FALSE;
            }
            break;

#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
        case LED_DIAG_REQUEST_OPEN_SHORT:
            result = LedApp_CopyOpenShortStatus();
            break;
#endif

        case LED_DIAG_REQUEST_NONE:
        case LED_DIAG_REQUEST_MAX:
        default:
            result = STD_NOT_OK;
            break;
    }

    LedApp_Context.diagnosticRequest = LED_DIAG_REQUEST_NONE;

    return result;
}

#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
/** \brief Copies the four Open/Short status words into named context fields. */
static Std_ReturnType LedApp_CopyOpenShortStatus(void)
{
    Std_ReturnType result;
    uint16 status[LED_OPEN_SHORT_STATUS_COUNT];

    result = E52248_Comm_CopyReadData(
        status,
        LED_OPEN_SHORT_STATUS_COUNT);

    if (result == STD_OK)
    {
        LedApp_Context.openStatus0To7 = status[0];
        LedApp_Context.openStatus8To15 = status[1];
        LedApp_Context.shortStatus0To7 = status[2];
        LedApp_Context.shortStatus8To15 = status[3];
        LedApp_Context.openShortStatusValid = TRUE;
    }
    else
    {
        LedApp_Context.openShortStatusValid = FALSE;
    }

    return result;
}
#endif

