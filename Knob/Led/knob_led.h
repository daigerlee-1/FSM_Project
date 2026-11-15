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
    \file   knob_led.h
    \brief  Public interface for the LED application FSM.
            Defines LED states, events, context data, and request APIs.
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
| 2026-10-15 000.02     DL      Standardized public header documentation
| 2026-10-15 000.03     DL      Added runtime LED setting requests
| 2026-10-20 000.04     DL      Added VLED and temperature read requests
| 2026-10-20 000.05     DL      Added VDIF and ILED read requests
| 2026-10-30 000.06     DL      Added observable LED output-enable state
| 2026-10-30 000.07     DL      Removed power events and toggle control
| 2026-11-08 000.08     DL      Added guarded runtime OTP test-write request
| 2026-11-09 000.09     DL      Added observable OTP programming address
| 2026-11-09 000.10     DL      Added optional Open/Short status test
********************************************************************************/
#ifndef KNOB_LED_H
#define KNOB_LED_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_defs.h"
#include "prj_typedefs.h"

#include "e52248_comm.h"
#include "fsm.h"
#include "knob_led_cfg.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* LED configuration limits are declared in knob_led_cfg.h. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/**
 * \brief Identifies the current LED application FSM state.
 *
 * LED_STATE_MAX is an out-of-range sentinel and shall not be used as an
 * active state.
 */
typedef enum
{
    LED_STATE_RESET = 0U,
    LED_STATE_I2C_INIT,
    LED_STATE_ADDR_CHK,
    LED_STATE_OTP_RESET,
    LED_STATE_OTP_READY_WAIT,
    LED_STATE_OTP_WRITE,
    LED_STATE_OTP_VALIDITY,
    LED_STATE_CONFIG_RESET,
    LED_STATE_CONFIG_READY_WAIT,
    LED_STATE_SET_CURRENT,
    LED_STATE_SET_PORT,
    LED_STATE_SET_BRIGHT,
    LED_STATE_NORMAL,
    LED_STATE_FAULT,
    LED_STATE_MAX
} Led_StateType;

/**
 * \brief Identifies an event accepted by the LED application FSM.
 *
 * LED_EVENT_MAX is an out-of-range sentinel and shall not be posted.
 */
typedef enum
{
    LED_EVENT_RESET_ON = 0U,
    LED_EVENT_I2C_ON,
    LED_EVENT_ADDR_OK,
    LED_EVENT_RESET_DONE,
    LED_EVENT_OTP_READY,
    LED_EVENT_OTP_VALID_REQ,
#if (PRJ_LED_OTP_TEST_WRITE_ENABLE == 1U)
    LED_EVENT_OTP_WRITE_REQ,
#endif
    LED_EVENT_OTP_INVALID,
    LED_EVENT_WRITE_OK,
    LED_EVENT_UPDATE_OK,
    LED_EVENT_CONFIG_READY,
    LED_EVENT_COMM_ERROR,
    LED_EVENT_CUR_REQ,
    LED_EVENT_PORT_REQ,
    LED_EVENT_BRIGHT_REQ,
    LED_EVENT_FAULT,
    LED_EVENT_RESET,
    LED_EVENT_MAX
} Led_EventType;

/** \brief Identifies whether a setting write belongs to startup or runtime. */
typedef enum
{
    LED_UPDATE_MODE_INIT = 0U,
    LED_UPDATE_MODE_RUNTIME,
    LED_UPDATE_MODE_MAX
} Led_UpdateModeType;

/** \brief Identifies an asynchronous diagnostic register-read request. */
typedef enum
{
    LED_DIAG_REQUEST_NONE = 0U,
    LED_DIAG_REQUEST_VLED,
    LED_DIAG_REQUEST_VDIF,
    LED_DIAG_REQUEST_ILED,
    LED_DIAG_REQUEST_TEMPERATURE,
#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
    LED_DIAG_REQUEST_OPEN_SHORT,
#endif
    LED_DIAG_REQUEST_MAX
} Led_DiagnosticRequestType;

/** \brief Contains the observable state of the LED application module. */
typedef struct
{
    /** State captured during the latest LED main-function execution. */
    Led_StateType state;

    /** Event processed during the latest LED main-function execution. */
    Led_EventType event;

    /** Determines the completion path of current, port, and brightness
     * writes. */
    Led_UpdateModeType updateMode;

    /** Prevents more than one runtime setting request from being queued. */
    boolean settingRequestPending;

    /** Indicates whether the E52248 output is currently enabled. */
    boolean outputEnabled;

    /** Address selected for the active OTP programming sequence. */
    uint8 otpSlaveAddress;

    /** Indicates that OTP write and post-write validation are active. */
    boolean otpProgrammingActive;

    /** Result of the latest complete OTP address-and-value validation. */
    boolean otpValidationValid;

    /** Configuration-table index most recently compared. */
    uint8 otpValidationIndex;

    /** Expected OTP value at the most recently compared address. */
    uint16 otpValidationExpectedValue;

    /** Actual OTP value read from the most recently compared address. */
    uint16 otpValidationActualValue;

    /** E52248 10-bit brightness value used by the next brightness write. */
    uint16 brightnessValue;

    /** E52248 10-bit current values indexed by LED channel. */
    uint16 currentValues[LED_CHANNEL_COUNT];

    /** Active asynchronous diagnostic read, or NONE when idle. */
    Led_DiagnosticRequestType diagnosticRequest;

    /** Latest raw VLED measurements indexed by channels 0 through 15. */
    uint16 vledValues[LED_CHANNEL_COUNT];

    /** Indicates that vledValues contains a completed read result. */
    boolean vledValuesValid;

    /** Latest raw VDIF measurements indexed by channels 0 through 15. */
    uint16 vdifValues[LED_CHANNEL_COUNT];

    /** Indicates that vdifValues contains a completed read result. */
    boolean vdifValuesValid;

    /** Latest raw ILED measurements indexed by channels 0 through 15. */
    uint16 iledValues[LED_CHANNEL_COUNT];

    /** Indicates that iledValues contains a completed read result. */
    boolean iledValuesValid;

    /** Latest raw E52248 junction-temperature register value. */
    uint16 temperatureRaw;

    /** Latest junction temperature converted from Kelvin to Celsius. */
    sint16 temperatureCelsius;

    /** Indicates that both temperature values contain a completed result. */
    boolean temperatureValid;

#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
    /** Open-fault status bits for LED channels 0 through 7. */
    uint16 openStatus0To7;

    /** Open-fault status bits for LED channels 8 through 15. */
    uint16 openStatus8To15;

    /** Short-fault status bits for LED channels 0 through 7. */
    uint16 shortStatus0To7;

    /** Short-fault status bits for LED channels 8 through 15. */
    uint16 shortStatus8To15;

    /** Indicates that all four Open/Short status words are current. */
    boolean openShortStatusValid;
#endif

    /** Generic FSM runtime context owned by the LED application. */
    Fsm_ContextType fsm;

    /** Read-only diagnostic view of the E52248 communication context. */
    const E52248_ContextType *ctrl_ic;
} Led_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Initializes the LED driver, communication module, and application FSM.
 */
void LedApp_Init(void);

/**
 * \brief Executes the 5 ms LED communication processing.
 *
 * This function advances the asynchronous E52248 communication FSM and
 * converts completed jobs into LED application events.
 */
void LedApp_MainFunction5ms(void);

/**
 * \brief Executes the 10 ms LED application processing.
 *
 * This function checks LED timers, dispatches one queued event, and executes
 * the active state's periodic operation.
 */
void LedApp_MainFunction10ms(void);

/**
 * \brief Enables the LED output after a valid brightness setting completes.
 *
 * The output remains enabled until explicitly disabled, a fault occurs, or the
 * module is reinitialized.
 *
 * \return STD_OK when enabled in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_EnableOutput(void);

/**
 * \brief Disables the E52248 LED output immediately.
 *
 * The latched output state is cleared so completion of an in-progress setting
 * request cannot re-enable the output.
 */
void LedApp_DisableOutput(void);

/**
 * \brief Reports whether the E52248 output is currently enabled.
 * \return TRUE when the output is enabled; otherwise FALSE.
 */
boolean LedApp_IsOutputEnabled(void);

/**
 * \brief Requests a runtime update of one port or all port-current values.
 *
 * Port numbers 0 through 15 select an individual port. LED_PORT_MAX selects
 * all ports. The current value shall be in the E52248 10-bit range 0 through
 * 1023.
 *
 * \param[in] portNumber Individual port number or LED_PORT_MAX for all ports.
 * \param[in] currentValue E52248 10-bit current value to apply.
 * \return STD_OK when accepted in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestSetCurrent(uint8 portNumber, uint16 currentValue);

/**
 * \brief Requests the configured LED port values at runtime.
 *
 * \return STD_OK when accepted in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestSetPort(void);

/**
 * \brief Requests a runtime brightness update using a CLU brightness step.
 *
 * Steps 2 through 100 use the configured duty table. Steps 101 through 200
 * select 100 percent duty. Steps outside this range are rejected.
 *
 * \param[in] brightnessStep CLU automatic-brightness step from 2 to 200.
 * \return STD_OK when accepted in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestSetBrightness(uint8 brightnessStep);

/**
 * \brief Requests validation of every configured OTP address while NORMAL.
 *
 * \return STD_OK when queued in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestOtpValidation(void);

#if (PRJ_LED_OTP_TEST_WRITE_ENABLE == 1U)
/**
 * \brief Requests one test-only OTP programming sequence while NORMAL.
 *
 * The request disables LED output and runs the complete PROG/reset, OTP
 * write, validation, normal-reset, and runtime-configuration sequence. Only
 * one request is accepted during each boot.
 *
 * \return STD_OK when queued in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestOtpWrite(void);
#endif

/**
 * \brief Requests all 16 VLED result registers while the LED FSM is normal.
 *
 * The asynchronous result is stored in Led_ContextType::vledValues.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestReadVled(void);

/**
 * \brief Requests all 16 VDIF result registers while the LED FSM is normal.
 *
 * The asynchronous raw result is stored in Led_ContextType::vdifValues.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestReadVdif(void);

/**
 * \brief Requests all 16 ILED result registers while the LED FSM is normal.
 *
 * The asynchronous raw result is stored in Led_ContextType::iledValues.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestReadIled(void);

/**
 * \brief Requests the E52248 junction-temperature result register.
 *
 * The asynchronous raw and Celsius results are stored in Led_ContextType.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestReadTemperature(void);

#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
/**
 * \brief Requests the four consecutive LED Open/Short status registers.
 *
 * The result is stored in the Open/Short fields of Led_ContextType.
 *
 * \return STD_OK when accepted in LED_STATE_NORMAL; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestReadOpenShort(void);
#endif

/**
 * \brief Posts a fault request to the LED FSM.
 *
 * \return STD_OK when queued successfully; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestFault(void);

/**
 * \brief Posts a reset request to the LED FSM.
 *
 * \return STD_OK when queued successfully; otherwise STD_NOT_OK.
 */
Std_ReturnType LedApp_RequestReset(void);

/**
 * \brief Returns the LED state captured during the latest 10 ms execution.
 *
 * \return Current observable LED application state.
 */
Led_StateType LedApp_GetState(void);

/**
 * \brief Returns a read-only diagnostic view of the LED module context.
 *
 * \return Pointer to the statically allocated LED context.
 */
const Led_ContextType * LedApp_GetContext(void);

#endif /* KNOB_LED_H */
