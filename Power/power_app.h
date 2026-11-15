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
    \file   power_app.h
    \brief  Public interface for the power-management application FSM.
            Defines power states, events, observable context data, sampled
            voltage inputs, output requests, and event-posting APIs.
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
| 2026-10-21 000.02     DL      Standardized public header documentation
| 2026-10-21 000.03     DL      Added RTE-independent power I/O interfaces
| 2026-10-21 000.04     DL      Added battery-based CAN TX permission
| 2026-10-21 000.05     DL      Added voltage hysteresis timer states
| 2026-10-21 000.06     DL      Generalized battery operating-voltage status
| 2026-10-22 000.07     DL      Added CCP/RRC power-state model
| 2026-10-29 000.08     DL      Added controlled LCAN sleep-entry state
| 2026-10-29 000.09     DL      Ordered states by power-control sequence
| 2026-10-29 000.10     DL      Routed SLEEP through final SLEEP_IN state
| 2026-10-29 000.11     DL      Used internal state values for CAN and NVM
| 2026-11-12 000.12     DL      Added confirmed ACC-state query API
********************************************************************************/
#ifndef POWER_APP_H
#define POWER_APP_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "fsm.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/**
 * \brief Identifies the current power-management FSM state.
 *
 * POWER_STATE_MAX is an out-of-range sentinel and shall not be used as an
 * active state.
 */
typedef enum
{
    POWER_STATE_INIT = 0U,
    POWER_STATE_SLEEP,
    POWER_STATE_SLEEP_IN,
    POWER_STATE_STANDBY,
    POWER_STATE_POWER_ON,
    POWER_STATE_POWER_OFF,
    POWER_STATE_ERROR,
    POWER_STATE_MAX
} Power_StateType;

/**
 * \brief Identifies an event accepted by the power-management FSM.
 *
 * POWER_EVENT_MAX is an out-of-range sentinel and shall not be posted.
 */
typedef enum
{
    POWER_EVENT_INIT_DONE = 0U,
    POWER_EVENT_ACC_ON,
    POWER_EVENT_ACC_OFF,
    POWER_EVENT_CAN_RX,
    POWER_EVENT_HU_SLEEP,
    POWER_EVENT_HU_POWER_OFF,
    POWER_EVENT_HU_POWER_ON,
    POWER_EVENT_HU_TIMEOUT,
    POWER_EVENT_SAFETY_MODE,
    POWER_EVENT_FAULT,
    POWER_EVENT_RESET,
    POWER_EVENT_SLEEP_READY,
    POWER_EVENT_MAX
} Power_EventType;

/** \brief HU power-state values transported by the CAN signal. */
typedef enum
{
    POWER_HU_STATE_SLEEP = 0U,
    POWER_HU_STATE_POWER_OFF,
    POWER_HU_STATE_POWER_ON,
    POWER_HU_STATE_INVALID,
    POWER_HU_STATE_MAX
} Power_HuStateType;

/** \brief Contains the observable state of the power application module. */
typedef struct
{
    /** State captured during the latest power main-function execution. */
    Power_StateType state;

    /** Event processed during the latest power main-function execution. */
    Power_EventType event;

    /** Latest valid ACC ADC raw value supplied by the integration layer. */
    uint32 accVoltage;

    /** Latest valid battery ADC raw value supplied by the integration layer. */
    uint32 batteryVoltage;

    /** Indicates that at least one ACC-voltage sample has been supplied. */
    boolean accVoltageValid;

    /** Indicates that at least one battery-voltage sample has been supplied. */
    boolean batteryVoltageValid;

    /** ACC state whose transition was accepted by the FSM event queue. */
    boolean accOn;

    /** Requested external power-output state. */
    boolean outputEnabled;

    /** Common operating state derived from the battery ADC value. */
    boolean batteryOperationOn;

    /** Indicates that the ACC hysteresis confirmation timer is running. */
    boolean accHysteresisTimerActive;

    /** Indicates that the battery hysteresis confirmation timer is running. */
    boolean batteryHysteresisTimerActive;

    /** Most recently accepted valid HU power-state value. */
    Power_HuStateType huPowerState;

    /** Indicates that at least one valid HU power-state value was received. */
    boolean huPowerStateValid;

    /** Indicates that the state-specific HU message timeout has elapsed. */
    boolean huMessageTimedOut;

    /** Indicates that the state-specific HU timeout timer is running. */
    boolean huTimeoutTimerActive;

    /** Generic table-driven FSM runtime context. */
    Fsm_ContextType fsm;
} Power_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** \brief Initializes the power context and table-driven FSM. */
void PowerApp_Init(void);

/** \brief Executes one 10 ms power input, timer, and FSM processing cycle. */
void PowerApp_MainFunction10ms(void);

/**
 * \brief Supplies the latest valid ACC ADC raw value.
 * \param[in] voltage ADC value used for the configured ACC thresholds.
 */
void PowerApp_SetAccVoltage(uint32 voltage);

/**
 * \brief Supplies the latest valid battery ADC raw value.
 * \param[in] voltage ADC value used for operating-voltage thresholds.
 */
void PowerApp_SetBatteryVoltage(uint32 voltage);

/**
 * \brief Queues an accepted ACC-ON change.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_RequestIgnitionOn(void);

/**
 * \brief Queues an accepted ACC-OFF change.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_RequestIgnitionOff(void);

/**
 * \brief Reports CAN reception, queueing a wake event from a sleep state.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_NotifyCanRx(void);

/**
 * \brief Supplies one received HU power-state value.
 * \param[in] huPowerState Raw HU power-state value from 0x0 through 0x3.
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_NotifyHuPowerState(uint8 huPowerState);

/**
 * \brief Reports expiration of the HU power-state message timeout.
 * \return STD_OK when queued or already reported; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_NotifyHuPowerStateTimeout(void);

/**
 * \brief Queues reception of the safety-mode test message.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_RequestSafetyMode(void);

/**
 * \brief Queues a power-fault request.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_RequestFault(void);

/**
 * \brief Queues a power-reset request.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_RequestReset(void);

/**
 * \brief Reports that LCAN NoCom and transceiver sleep preparation completed.
 * \return STD_OK when queued; otherwise STD_NOT_OK.
 */
Std_ReturnType PowerApp_RequestSleepReady(void);

/**
 * \brief Reports whether the power FSM is in its powered operating state.
 * \return TRUE in POWER_STATE_POWER_ON; otherwise FALSE.
 */
boolean PowerApp_IsOperational(void);

/**
 * \brief Returns the requested external power-output state.
 * \return TRUE when the output shall be enabled; otherwise FALSE.
 */
boolean PowerApp_IsOutputEnabled(void);

/**
 * \brief Reports the confirmed ACC state after hysteresis processing.
 * \return TRUE while ACC is confirmed ON; otherwise FALSE.
 */
boolean PowerApp_IsAccOn(void);

/** \brief Returns the latest synchronized power FSM state. */
Power_StateType PowerApp_GetState(void);

/**
 * \brief Returns read-only access to the power application context.
 * \return Pointer to the module-owned power context.
 */
const Power_ContextType * PowerApp_GetContext(void);

/**
 * \brief Reports the confirmed battery operating-voltage state.
 * \return TRUE while battery operation is ON; otherwise FALSE.
 */
boolean PowerApp_IsBatteryOperationOn(void);

#endif /* POWER_APP_H */
