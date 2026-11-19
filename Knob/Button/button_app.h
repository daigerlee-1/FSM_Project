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
    \file   button_app.h
    \brief  Declares the event-driven button application interface.
            Defines per-button states, events, report values, observable
            context data, cyclic processing, and report-transfer services.
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
| 2026-10-26 000.02     DL      Added debounce and short/long-key reporting
| 2026-10-26 000.03     DL      Standardized public header documentation
| 2026-11-02 000.04     DL      Changed the invalid signal value to 0x0F
| 2026-11-06 000.05     DL      Added logical outputs and stuck-switch diagnosis
********************************************************************************/
#ifndef BUTTON_APP_H
#define BUTTON_APP_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "drv_button.h"
#include "fsm.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Bit position of the button identifier in a packed uint16 report. */
#define BUTTON_REPORT_ID_SHIFT    ((uint8)8U)

/** Mask selecting the signal byte in a packed uint16 report. */
#define BUTTON_REPORT_SIGNAL_MASK ((uint16)0x00FFU)

/** Diagnostic value indicating that all button inputs are normal. */
#define BUTTON_DIAG_STATUS_NORMAL ((uint8)1U)

/** Diagnostic value indicating a continuously pressed button fault. */
#define BUTTON_DIAG_STATUS_STUCK  ((uint8)2U)

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Identifies the current debounce state of one button instance. */
typedef enum
{
    BUTTON_STATE_RELEASED = 0U,
    BUTTON_STATE_DEBOUNCE_PRESS,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_DEBOUNCE_RELEASE,
    BUTTON_STATE_MAX
} Button_StateType;

/** Identifies an event accepted by the button FSM. */
typedef enum
{
    BUTTON_EVENT_RAW_PRESS = 0U,
    BUTTON_EVENT_RAW_RELEASE,
    BUTTON_EVENT_DEBOUNCE_DONE,
    BUTTON_EVENT_MAX
} Button_EventType;

/** Identifies the CAN signal value reported for an individual button. */
typedef enum
{
    BUTTON_SIGNAL_CLEAR = 0U,
    BUTTON_SIGNAL_PRESSED,
    BUTTON_SIGNAL_SHORT_KEY,
    BUTTON_SIGNAL_LONG_KEY,
    BUTTON_SIGNAL_INVALID = 0x0FU,
    BUTTON_SIGNAL_MAX = 0x10U
} Button_SignalType;

/** Identifies one logical button output consumed by communication modules. */
typedef enum
{
    BUTTON_OUTPUT_APPS = 0U,
    BUTTON_OUTPUT_BACK,
    BUTTON_OUTPUT_SEARCH,
    BUTTON_OUTPUT_ENTER,
    BUTTON_OUTPUT_DIR_0,
    BUTTON_OUTPUT_DIR_45,
    BUTTON_OUTPUT_DIR_90,
    BUTTON_OUTPUT_DIR_135,
    BUTTON_OUTPUT_DIR_180,
    BUTTON_OUTPUT_DIR_225,
    BUTTON_OUTPUT_DIR_270,
    BUTTON_OUTPUT_DIR_315,
    BUTTON_OUTPUT_MAX
} Button_OutputType;

/** Contains the observable runtime state of one button instance. */
typedef struct
{
    /** Button instance associated with this context. */
    Button_IdType id;

    /** State synchronized from the common FSM context after dispatch. */
    Button_StateType state;

    /** Last event synchronized from the common FSM context. */
    Button_EventType event;

    /** Latest active-low input: 0U is pressed and 1U is released. */
    uint8 rawState;

    /** Time for which the current raw level has remained unchanged. */
    uint16 debounceElapsedMs;

    /** Saturating elapsed time measured from the initial press edge. */
    uint16 holdElapsedMs;

    /** Stable pressed duration used for stuck-switch diagnosis. */
    uint32 diagnosticHoldElapsedMs;

    /** Indicates that this switch exceeded the stuck-press threshold. */
    boolean stuckFault;

    /** Indicates that the long-key report was already generated. */
    boolean longKeyReported;

    /** Last signal removed from the queue after successful transmission. */
    Button_SignalType lastSignal;

    /** Saturating count of reports rejected because the queue was full. */
    uint16 reportQueueOverflowCount;

    /** Event queue and active state owned by the common FSM core. */
    Fsm_ContextType fsm;
} Button_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** Initializes the button driver, all button contexts, and the report queue. */
void ButtonApp_Init(void);

/** Processes all button instances using a 5 ms elapsed-time increment. */
void ButtonApp_MainFunction5ms(void);

/** Processes all button instances using a 10 ms elapsed-time increment. */
void ButtonApp_MainFunction10ms(void);

/**
 * \brief Retrieves the oldest pending button report without removing it.
 *
 * The report uses bits [15:8] for Button_OutputType and bits [7:0] for
 * Button_SignalType. Call ButtonApp_ConfirmReport only after the receiver has
 * accepted the returned value.
 *
 * \param[out] reportData Destination for the packed report value.
 * \return STD_OK when a report is available; otherwise STD_NOT_OK.
 */
Std_ReturnType ButtonApp_GetReport(uint16 * const reportData);

/** Removes the oldest report after successful transfer to the integration layer. */
void ButtonApp_ConfirmReport(void);

/**
 * \brief Returns a bit mask of all stably pressed buttons.
 * \return Bit N is set when Button_IdType value N is pressed.
 */
uint16 ButtonApp_GetPressedMask(void);

/**
 * \brief Returns the aggregated button-input diagnostic status.
 * \return BUTTON_DIAG_STATUS_STUCK when any switch is stuck; otherwise normal.
 */
uint8 ButtonApp_GetDiagnosticStatus(void);

/**
 * \brief Returns read-only access to the default APPS-button context.
 * \return Pointer to the module-owned APPS-button context.
 */
const Button_ContextType * ButtonApp_GetContext(void);

/**
 * \brief Returns read-only access to a selected button context.
 * \param[in] buttonId Button instance to inspect.
 * \return Context pointer for a valid ID; otherwise NULL_PTR.
 */
const Button_ContextType * ButtonApp_GetContextById(Button_IdType buttonId);

#endif /* BUTTON_APP_H */
