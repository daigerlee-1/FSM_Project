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
    \file   button_app.c
    \brief  Implements debounce, short/long-key, and logical-button processing.
            Each physical button owns an FSM. Enter contacts and wheel-direction
            combinations are converted into buffered logical reports for RTE.
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
| 2026-10-26 000.02     DL      Added per-button debounce and key reporting
| 2026-10-26 000.03     DL      Standardized C source documentation
| 2026-11-02 000.04     DL      Validated the sparse button signal values
| 2026-11-02 000.05     DL      Removed the legacy aggregate CAN button state
| 2026-11-06 000.06     DL      Added logical outputs and stuck-switch diagnosis
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "button_app.h"

#include "drv_button.h"
#include "fsm.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Time for which a raw level must remain stable before it is accepted. */
#define BUTTON_DEBOUNCE_TIME_MS ((uint16)30U)

/** Press duration at which a long-key report is generated. */
#define BUTTON_LONG_KEY_TIME_MS ((uint16)800U)

/** Stable press duration at which one switch is diagnosed as stuck. */
#define BUTTON_STUCK_TIME_MS    ((uint32)660000U)

/** Elapsed-time increment used by ButtonApp_MainFunction5ms. */
#define BUTTON_TASK_5MS         ((uint16)5U)

/** Elapsed-time increment used by ButtonApp_MainFunction10ms. */
#define BUTTON_TASK_10MS        ((uint16)10U)

/** Maximum number of reports buffered for integration-layer transfer. */
#define BUTTON_REPORT_QUEUE_SIZE ((uint8)32U)

/** Saturation limit for elapsed-time and diagnostic counters. */
#define BUTTON_UINT16_MAX        ((uint16)0xFFFFU)

/** Stable-pressed mask bits for the grouped Enter contacts. */
#define BUTTON_ENTER1_MASK                                      \
    ((uint16)((uint16)1U << (uint16)BUTTON_ID_ENTER1))
#define BUTTON_ENTER2_MASK                                      \
    ((uint16)((uint16)1U << (uint16)BUTTON_ID_ENTER2))
#define BUTTON_ENTER_MASK                                       \
    ((uint16)(BUTTON_ENTER1_MASK | BUTTON_ENTER2_MASK))

/** Stable-pressed mask bits for the four physical direction buttons. */
#define BUTTON_DIRECTION_DOWN_MASK                              \
    ((uint16)((uint16)1U << (uint16)BUTTON_ID_WHEEL_DOWN))
#define BUTTON_DIRECTION_LEFT_MASK                              \
    ((uint16)((uint16)1U << (uint16)BUTTON_ID_WHEEL_LEFT))
#define BUTTON_DIRECTION_RIGHT_MASK                             \
    ((uint16)((uint16)1U << (uint16)BUTTON_ID_WHEEL_RIGHT))
#define BUTTON_DIRECTION_UP_MASK                                \
    ((uint16)((uint16)1U << (uint16)BUTTON_ID_WHEEL_UP))
#define BUTTON_DIRECTION_MASK                                   \
    ((uint16)(BUTTON_DIRECTION_DOWN_MASK |                      \
              BUTTON_DIRECTION_LEFT_MASK |                      \
              BUTTON_DIRECTION_RIGHT_MASK |                    \
              BUTTON_DIRECTION_UP_MASK))

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Internal unpacked representation of one queued button report. */
typedef struct
{
    Button_OutputType outputId;
    Button_IdType ownerId;
    Button_SignalType signal;
} Button_ReportType;

/** Runtime data for one logical output assembled from physical buttons. */
typedef struct
{
    Button_OutputType activeOutput;
    uint16 holdElapsedMs;
    boolean longKeyReported;
} Button_GroupContextType;

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void ButtonApp_EntryNoAction(void);
static void ButtonApp_ExitNoAction(void);
static void ButtonApp_DoNoAction(void);
static uint8 ButtonApp_GuardAlways(void);
static void ButtonApp_ActionNone(void);
static void ButtonApp_Process(uint16 elapsedMs);
static void ButtonApp_ProcessOne(Button_ContextType * const context,
                                 uint16 elapsedMs);
static void ButtonApp_UpdateHoldTime(Button_ContextType * const context,
                                     uint16 elapsedMs);
static void ButtonApp_UpdateStableState(Button_ContextType * const context,
                                        Button_StateType previousState);
static void ButtonApp_UpdateLongKey(Button_ContextType * const context);
static void ButtonApp_UpdateStuckDiagnostic(
    Button_ContextType * const context,
    uint16 elapsedMs);
static void ButtonApp_UpdateGroupedOutputs(uint16 elapsedMs);
static void ButtonApp_UpdateGroup(Button_GroupContextType * const group,
                                  Button_OutputType requestedOutput,
                                  uint16 elapsedMs);
static boolean ButtonApp_IsGroupedButton(Button_IdType buttonId);
static Button_OutputType ButtonApp_GetIndividualOutput(Button_IdType buttonId);
static Button_OutputType ButtonApp_GetEnterOutput(void);
static Button_OutputType ButtonApp_GetDirectionOutput(void);
static void ButtonApp_ReportQueueInit(void);
static Std_ReturnType ButtonApp_ReportQueuePush(
    Button_ContextType * const context,
    Button_OutputType outputId,
    Button_SignalType signal);
static boolean ButtonApp_IsOutputValid(Button_OutputType outputId);
static boolean ButtonApp_IsSignalValid(Button_SignalType signal);
static boolean ButtonApp_ReportQueuePeek(Button_ReportType * const report);
static void ButtonApp_ReportQueuePop(void);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Runtime context for every configured physical button. */
static Button_ContextType ButtonApp_Context[BUTTON_ID_MAX];

/** Bit mask representing all buttons in the stable pressed state. */
static uint16 ButtonApp_PressedMask;

/** Aggregated runtime state for the two physical Enter contacts. */
static Button_GroupContextType ButtonApp_EnterGroup;

/** Aggregated runtime state for the eight logical wheel directions. */
static Button_GroupContextType ButtonApp_DirectionGroup;

/** Saturating count of grouped reports rejected because the FIFO was full. */
static uint16 ButtonApp_GroupReportQueueOverflowCount;

/** FIFO used to decouple button processing from report transmission. */
static Button_ReportType ButtonApp_ReportQueue[BUTTON_REPORT_QUEUE_SIZE];

/** Index of the oldest report waiting for confirmation. */
static uint8 ButtonApp_ReportQueueHead;

/** Index at which the next report will be inserted. */
static uint8 ButtonApp_ReportQueueTail;

/** Number of valid reports currently stored in the FIFO. */
static uint8 ButtonApp_ReportQueueCount;

/*******************************************************************************
    9. PRIVATE CONSTANTS AND FSM CONFIGURATION
*******************************************************************************/
/** State-operation table shared by all button FSM instances. */
static const Fsm_StateOperationType ButtonApp_StateOperations[BUTTON_STATE_MAX] =
{
    { ButtonApp_EntryNoAction, ButtonApp_ExitNoAction, ButtonApp_DoNoAction },
    { ButtonApp_EntryNoAction, ButtonApp_ExitNoAction, ButtonApp_DoNoAction },
    { ButtonApp_EntryNoAction, ButtonApp_ExitNoAction, ButtonApp_DoNoAction },
    { ButtonApp_EntryNoAction, ButtonApp_ExitNoAction, ButtonApp_DoNoAction }
};

/** Transition table implementing press and release debounce behavior. */
static const Fsm_TransitionType ButtonApp_Transitions[] =
{
    { (Fsm_StateType)BUTTON_STATE_RELEASED,
      (Fsm_EventType)BUTTON_EVENT_RAW_PRESS,
      ButtonApp_GuardAlways, ButtonApp_ActionNone,
      (Fsm_StateType)BUTTON_STATE_DEBOUNCE_PRESS },
    { (Fsm_StateType)BUTTON_STATE_DEBOUNCE_PRESS,
      (Fsm_EventType)BUTTON_EVENT_RAW_RELEASE,
      ButtonApp_GuardAlways, ButtonApp_ActionNone,
      (Fsm_StateType)BUTTON_STATE_RELEASED },
    { (Fsm_StateType)BUTTON_STATE_DEBOUNCE_PRESS,
      (Fsm_EventType)BUTTON_EVENT_DEBOUNCE_DONE,
      ButtonApp_GuardAlways, ButtonApp_ActionNone,
      (Fsm_StateType)BUTTON_STATE_PRESSED },
    { (Fsm_StateType)BUTTON_STATE_PRESSED,
      (Fsm_EventType)BUTTON_EVENT_RAW_RELEASE,
      ButtonApp_GuardAlways, ButtonApp_ActionNone,
      (Fsm_StateType)BUTTON_STATE_DEBOUNCE_RELEASE },
    { (Fsm_StateType)BUTTON_STATE_DEBOUNCE_RELEASE,
      (Fsm_EventType)BUTTON_EVENT_RAW_PRESS,
      ButtonApp_GuardAlways, ButtonApp_ActionNone,
      (Fsm_StateType)BUTTON_STATE_PRESSED },
    { (Fsm_StateType)BUTTON_STATE_DEBOUNCE_RELEASE,
      (Fsm_EventType)BUTTON_EVENT_DEBOUNCE_DONE,
      ButtonApp_GuardAlways, ButtonApp_ActionNone,
      (Fsm_StateType)BUTTON_STATE_RELEASED }
};

/** Number of transition entries derived from the transition table. */
#define BUTTON_TRANSITION_COUNT                               \
    ((uint16)(sizeof(ButtonApp_Transitions) /                 \
              sizeof(ButtonApp_Transitions[0])))

/** Immutable FSM configuration shared by all button contexts. */
static const Fsm_ConfigType ButtonApp_Config =
{
    (Fsm_StateType)BUTTON_STATE_RELEASED,
    (Fsm_StateType)BUTTON_STATE_MAX,
    (Fsm_EventType)BUTTON_EVENT_MAX,
    ButtonApp_StateOperations,
    ButtonApp_Transitions,
    BUTTON_TRANSITION_COUNT
};

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes all button contexts, FSM instances, and report storage. */
void ButtonApp_Init(void)
{
    uint8 index;

    DrvButton_Init();
    ButtonApp_PressedMask = (uint16)0U;
    ButtonApp_EnterGroup.activeOutput = BUTTON_OUTPUT_MAX;
    ButtonApp_EnterGroup.holdElapsedMs = (uint16)0U;
    ButtonApp_EnterGroup.longKeyReported = FALSE;
    ButtonApp_DirectionGroup.activeOutput = BUTTON_OUTPUT_MAX;
    ButtonApp_DirectionGroup.holdElapsedMs = (uint16)0U;
    ButtonApp_DirectionGroup.longKeyReported = FALSE;
    ButtonApp_GroupReportQueueOverflowCount = (uint16)0U;
    ButtonApp_ReportQueueInit();

    for (index = (uint8)0U; index < (uint8)BUTTON_ID_MAX; index++)
    {
        ButtonApp_Context[index].id = (Button_IdType)index;
        ButtonApp_Context[index].state = BUTTON_STATE_RELEASED;
        ButtonApp_Context[index].event = BUTTON_EVENT_MAX;
        ButtonApp_Context[index].rawState = BUTTON_RAW_STATE_RELEASED;
        ButtonApp_Context[index].debounceElapsedMs = (uint16)0U;
        ButtonApp_Context[index].holdElapsedMs = (uint16)0U;
        ButtonApp_Context[index].diagnosticHoldElapsedMs = (uint32)0U;
        ButtonApp_Context[index].stuckFault = FALSE;
        ButtonApp_Context[index].longKeyReported = FALSE;
        ButtonApp_Context[index].lastSignal = BUTTON_SIGNAL_CLEAR;
        ButtonApp_Context[index].reportQueueOverflowCount = (uint16)0U;
        (void)Fsm_Init(&ButtonApp_Context[index].fsm, &ButtonApp_Config);
    }
}

/** \brief Processes every button using a 5 ms elapsed-time increment. */
void ButtonApp_MainFunction5ms(void)
{
    ButtonApp_Process(BUTTON_TASK_5MS);
}

/** \brief Processes every button using a 10 ms elapsed-time increment. */
void ButtonApp_MainFunction10ms(void)
{
    ButtonApp_Process(BUTTON_TASK_10MS);
}

/** \brief Peeks and packs the oldest pending report for transmission. */
Std_ReturnType ButtonApp_GetReport(uint16 * const reportData)
{
    Std_ReturnType result = STD_NOT_OK;
    Button_ReportType report;

    if ((reportData != NULL_PTR) &&
        (ButtonApp_ReportQueuePeek(&report) == TRUE))
    {
        *reportData =
            (uint16)(((uint16)report.outputId << BUTTON_REPORT_ID_SHIFT) |
                     ((uint16)report.signal & BUTTON_REPORT_SIGNAL_MASK));
        result = STD_OK;
    }
    else
    {
        /* No changed stable-button report is pending. */
    }

    return result;
}

/** \brief Confirms successful transmission and removes the oldest report. */
void ButtonApp_ConfirmReport(void)
{
    Button_ReportType report;

    if (ButtonApp_ReportQueuePeek(&report) == TRUE)
    {
        if ((uint8)report.ownerId < (uint8)BUTTON_ID_MAX)
        {
            ButtonApp_Context[(uint8)report.ownerId].lastSignal =
                report.signal;
        }
        else
        {
            /* Grouped logical reports do not belong to one physical button. */
        }

        ButtonApp_ReportQueuePop();
    }
    else
    {
        /* Ignore confirmation when no report is pending. */
    }
}

/** \brief Returns the bit mask of all stably pressed buttons. */
uint16 ButtonApp_GetPressedMask(void)
{
    return ButtonApp_PressedMask;
}

/** \brief Returns abnormal status when any physical switch is stuck pressed. */
uint8 ButtonApp_GetDiagnosticStatus(void)
{
    uint8 diagnosticStatus = BUTTON_DIAG_STATUS_NORMAL;
    uint8 index;

    for (index = (uint8)0U; index < (uint8)BUTTON_ID_MAX; index++)
    {
        if (ButtonApp_Context[index].stuckFault == TRUE)
        {
            diagnosticStatus = BUTTON_DIAG_STATUS_STUCK;
            break;
        }
        else
        {
            /* Continue checking the remaining physical switches. */
        }
    }

    return diagnosticStatus;
}

/** \brief Returns read-only access to the APPS-button context. */
const Button_ContextType * ButtonApp_GetContext(void)
{
    return &ButtonApp_Context[(uint8)BUTTON_ID_APPS];
}

/** \brief Returns read-only access to a selected button context. */
const Button_ContextType * ButtonApp_GetContextById(Button_IdType buttonId)
{
    const Button_ContextType * context = NULL_PTR;

    if ((uint8)buttonId < (uint8)BUTTON_ID_MAX)
    {
        context = &ButtonApp_Context[(uint8)buttonId];
    }
    else
    {
        /* Return NULL_PTR for an invalid button identifier. */
    }

    return context;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Processes every button context using the supplied elapsed time. */
static void ButtonApp_Process(uint16 elapsedMs)
{
    uint8 index;

    for (index = (uint8)0U; index < (uint8)BUTTON_ID_MAX; index++)
    {
        ButtonApp_ProcessOne(&ButtonApp_Context[index], elapsedMs);
    }

    ButtonApp_UpdateGroupedOutputs(elapsedMs);
}

/**
 * \brief Samples, debounces, and dispatches one button FSM instance.
 * \param[in,out] context Button instance being processed.
 * \param[in] elapsedMs Time elapsed since the previous invocation.
 */
static void ButtonApp_ProcessOne(Button_ContextType * const context,
                                 uint16 elapsedMs)
{
    uint8 rawState = DrvButton_ReadById(context->id);
    Button_StateType previousState = context->state;

    if (rawState != context->rawState)
    {
        context->rawState = rawState;
        context->debounceElapsedMs = (uint16)0U;

        if (rawState == BUTTON_RAW_STATE_PRESSED)
        {
            if (context->state == BUTTON_STATE_RELEASED)
            {
                context->holdElapsedMs = (uint16)0U;
                context->longKeyReported = FALSE;
            }
            else
            {
                /* Preserve hold time when a release edge was chattering. */
            }

            (void)Fsm_PostEvent(&context->fsm,
                                (Fsm_EventType)BUTTON_EVENT_RAW_PRESS);
        }
        else if (rawState == BUTTON_RAW_STATE_RELEASED)
        {
            (void)Fsm_PostEvent(&context->fsm,
                                (Fsm_EventType)BUTTON_EVENT_RAW_RELEASE);
        }
        else
        {
            /* The driver rejects raw levels outside the active-low domain. */
        }
    }
    else if ((context->state == BUTTON_STATE_DEBOUNCE_PRESS) ||
             (context->state == BUTTON_STATE_DEBOUNCE_RELEASE))
    {
        context->debounceElapsedMs =
            (uint16)(context->debounceElapsedMs + elapsedMs);

        if (context->debounceElapsedMs >= BUTTON_DEBOUNCE_TIME_MS)
        {
            context->debounceElapsedMs = (uint16)0U;
            (void)Fsm_PostEvent(&context->fsm,
                                (Fsm_EventType)BUTTON_EVENT_DEBOUNCE_DONE);
        }
        else
        {
            /* Continue the debounce interval. */
        }
    }
    else
    {
        context->debounceElapsedMs = (uint16)0U;
    }

    ButtonApp_UpdateHoldTime(context, elapsedMs);
    Fsm_Dispatch(&context->fsm);
    Fsm_Do(&context->fsm);
    context->state = (Button_StateType)Fsm_GetState(&context->fsm);
    context->event = (Button_EventType)Fsm_GetLastEvent(&context->fsm);
    ButtonApp_UpdateStableState(context, previousState);
    ButtonApp_UpdateLongKey(context);
    ButtonApp_UpdateStuckDiagnostic(context, elapsedMs);
}

/**
 * \brief Updates continuous stable-press timing for stuck-switch diagnosis.
 * \param[in,out] context Button instance whose diagnosis is updated.
 * \param[in] elapsedMs Time elapsed since the previous invocation.
 */
static void ButtonApp_UpdateStuckDiagnostic(
    Button_ContextType * const context,
    uint16 elapsedMs)
{
    uint32 nextElapsedMs;

    if (context->state == BUTTON_STATE_PRESSED)
    {
        nextElapsedMs = context->diagnosticHoldElapsedMs +
                        (uint32)elapsedMs;

        if (nextElapsedMs >= BUTTON_STUCK_TIME_MS)
        {
            context->diagnosticHoldElapsedMs = BUTTON_STUCK_TIME_MS;
            context->stuckFault = TRUE;
        }
        else
        {
            context->diagnosticHoldElapsedMs = nextElapsedMs;
            context->stuckFault = FALSE;
        }
    }
    else if (context->state == BUTTON_STATE_DEBOUNCE_RELEASE)
    {
        /* Preserve the diagnosis while a possible release is debounced. */
    }
    else
    {
        context->diagnosticHoldElapsedMs = (uint32)0U;
        context->stuckFault = FALSE;
    }
}

/**
 * \brief Accumulates the press duration with uint16 saturation.
 * \param[in,out] context Button instance whose hold time is updated.
 * \param[in] elapsedMs Time elapsed since the previous invocation.
 */
static void ButtonApp_UpdateHoldTime(Button_ContextType * const context,
                                     uint16 elapsedMs)
{
    uint32 nextElapsedMs;

    if (context->rawState == BUTTON_RAW_STATE_PRESSED)
    {
        nextElapsedMs = (uint32)context->holdElapsedMs + (uint32)elapsedMs;

        if (nextElapsedMs <= (uint32)BUTTON_UINT16_MAX)
        {
            context->holdElapsedMs = (uint16)nextElapsedMs;
        }
        else
        {
            context->holdElapsedMs = BUTTON_UINT16_MAX;
        }
    }
    else
    {
        /* Preserve the measured hold time through release debouncing. */
    }
}

/**
 * \brief Handles reports and local effects caused by a stable-state change.
 * \param[in,out] context Button instance whose state was dispatched.
 * \param[in] previousState State observed before the current dispatch.
 */
static void ButtonApp_UpdateStableState(Button_ContextType * const context,
                                        Button_StateType previousState)
{
    uint16 buttonMask =
        (uint16)((uint16)1U << (uint16)context->id);
    Button_OutputType outputId;

    if ((context->state == BUTTON_STATE_PRESSED) &&
        (previousState == BUTTON_STATE_DEBOUNCE_PRESS))
    {
        ButtonApp_PressedMask = (uint16)(ButtonApp_PressedMask | buttonMask);

        if (ButtonApp_IsGroupedButton(context->id) == FALSE)
        {
            outputId = ButtonApp_GetIndividualOutput(context->id);
            (void)ButtonApp_ReportQueuePush(context, outputId,
                                            BUTTON_SIGNAL_PRESSED);
            (void)ButtonApp_ReportQueuePush(context, outputId,
                                            BUTTON_SIGNAL_INVALID);
        }
        else
        {
            /* Grouped outputs are evaluated after all physical FSMs run. */
        }
    }
    else if ((context->state == BUTTON_STATE_RELEASED) &&
             (previousState == BUTTON_STATE_DEBOUNCE_RELEASE))
    {
        ButtonApp_PressedMask =
            (uint16)(ButtonApp_PressedMask & (uint16)(~buttonMask));

        if (ButtonApp_IsGroupedButton(context->id) == FALSE)
        {
            outputId = ButtonApp_GetIndividualOutput(context->id);
            if (context->longKeyReported == FALSE)
            {
                (void)ButtonApp_ReportQueuePush(context, outputId,
                                                BUTTON_SIGNAL_SHORT_KEY);
            }
            else
            {
                /* A long-key signal was already reported while held. */
            }

            (void)ButtonApp_ReportQueuePush(context, outputId,
                                            BUTTON_SIGNAL_CLEAR);
        }
        else
        {
            /* Grouped outputs are evaluated after all physical FSMs run. */
        }

        context->holdElapsedMs = (uint16)0U;
        context->longKeyReported = FALSE;
    }
    else
    {
        /* No stable button state changed. */
    }
}

/**
 * \brief Generates one long-key pulse when the hold threshold is crossed.
 * \param[in,out] context Button instance being evaluated.
 */
static void ButtonApp_UpdateLongKey(Button_ContextType * const context)
{
    if ((context->state == BUTTON_STATE_PRESSED) &&
        (ButtonApp_IsGroupedButton(context->id) == FALSE) &&
        (context->longKeyReported == FALSE) &&
        (context->holdElapsedMs >= BUTTON_LONG_KEY_TIME_MS))
    {
        Button_OutputType outputId =
            ButtonApp_GetIndividualOutput(context->id);

        context->longKeyReported = TRUE;
        (void)ButtonApp_ReportQueuePush(context, outputId,
                                        BUTTON_SIGNAL_LONG_KEY);
        (void)ButtonApp_ReportQueuePush(context, outputId,
                                        BUTTON_SIGNAL_INVALID);
    }
    else
    {
        /* No new long-key threshold crossing occurred. */
    }
}

/** \brief Evaluates logical outputs after every physical button was processed. */
static void ButtonApp_UpdateGroupedOutputs(uint16 elapsedMs)
{
    ButtonApp_UpdateGroup(&ButtonApp_EnterGroup,
                          ButtonApp_GetEnterOutput(),
                          elapsedMs);
    ButtonApp_UpdateGroup(&ButtonApp_DirectionGroup,
                          ButtonApp_GetDirectionOutput(),
                          elapsedMs);
}

/** \brief Generates reports for one logical multi-button output. */
static void ButtonApp_UpdateGroup(Button_GroupContextType * const group,
                                  Button_OutputType requestedOutput,
                                  uint16 elapsedMs)
{
    uint32 nextElapsedMs;

    if (requestedOutput != group->activeOutput)
    {
        if (ButtonApp_IsOutputValid(group->activeOutput) == TRUE)
        {
            if (group->longKeyReported == FALSE)
            {
                (void)ButtonApp_ReportQueuePush(NULL_PTR,
                                                group->activeOutput,
                                                BUTTON_SIGNAL_SHORT_KEY);
            }
            else
            {
                /* The previous logical output already reported a long key. */
            }

            (void)ButtonApp_ReportQueuePush(NULL_PTR,
                                            group->activeOutput,
                                            BUTTON_SIGNAL_CLEAR);
        }
        else
        {
            /* No previous logical output needs to be released. */
        }

        group->activeOutput = requestedOutput;
        group->holdElapsedMs = (uint16)0U;
        group->longKeyReported = FALSE;

        if (ButtonApp_IsOutputValid(requestedOutput) == TRUE)
        {
            (void)ButtonApp_ReportQueuePush(NULL_PTR,
                                            requestedOutput,
                                            BUTTON_SIGNAL_PRESSED);
            (void)ButtonApp_ReportQueuePush(NULL_PTR,
                                            requestedOutput,
                                            BUTTON_SIGNAL_INVALID);
        }
        else
        {
            /* No new logical output is active. */
        }
    }
    else if (ButtonApp_IsOutputValid(group->activeOutput) == TRUE)
    {
        nextElapsedMs =
            (uint32)group->holdElapsedMs + (uint32)elapsedMs;
        if (nextElapsedMs <= (uint32)BUTTON_UINT16_MAX)
        {
            group->holdElapsedMs = (uint16)nextElapsedMs;
        }
        else
        {
            group->holdElapsedMs = BUTTON_UINT16_MAX;
        }

        if ((group->longKeyReported == FALSE) &&
            (group->holdElapsedMs >= BUTTON_LONG_KEY_TIME_MS))
        {
            group->longKeyReported = TRUE;
            (void)ButtonApp_ReportQueuePush(NULL_PTR,
                                            group->activeOutput,
                                            BUTTON_SIGNAL_LONG_KEY);
            (void)ButtonApp_ReportQueuePush(NULL_PTR,
                                            group->activeOutput,
                                            BUTTON_SIGNAL_INVALID);
        }
        else
        {
            /* The logical long-key threshold has not just been crossed. */
        }
    }
    else
    {
        group->holdElapsedMs = (uint16)0U;
        group->longKeyReported = FALSE;
    }
}

/** \brief Checks whether a physical input belongs to a grouped output. */
static boolean ButtonApp_IsGroupedButton(Button_IdType buttonId)
{
    boolean result;

    switch (buttonId)
    {
        case BUTTON_ID_ENTER1:
        case BUTTON_ID_ENTER2:
        case BUTTON_ID_WHEEL_DOWN:
        case BUTTON_ID_WHEEL_LEFT:
        case BUTTON_ID_WHEEL_RIGHT:
        case BUTTON_ID_WHEEL_UP:
            result = TRUE;
            break;

        case BUTTON_ID_APPS:
        case BUTTON_ID_BACK:
        case BUTTON_ID_SEARCH:
        case BUTTON_ID_MAX:
        default:
            result = FALSE;
            break;
    }

    return result;
}

/** \brief Maps one non-grouped physical button to its logical output. */
static Button_OutputType ButtonApp_GetIndividualOutput(Button_IdType buttonId)
{
    Button_OutputType outputId;

    switch (buttonId)
    {
        case BUTTON_ID_APPS:
            outputId = BUTTON_OUTPUT_APPS;
            break;

        case BUTTON_ID_BACK:
            outputId = BUTTON_OUTPUT_BACK;
            break;

        case BUTTON_ID_SEARCH:
            outputId = BUTTON_OUTPUT_SEARCH;
            break;

        case BUTTON_ID_ENTER1:
        case BUTTON_ID_ENTER2:
        case BUTTON_ID_WHEEL_DOWN:
        case BUTTON_ID_WHEEL_LEFT:
        case BUTTON_ID_WHEEL_RIGHT:
        case BUTTON_ID_WHEEL_UP:
        case BUTTON_ID_MAX:
        default:
            outputId = BUTTON_OUTPUT_MAX;
            break;
    }

    return outputId;
}

/** \brief Resolves the two physical Enter contacts to one logical output. */
static Button_OutputType ButtonApp_GetEnterOutput(void)
{
    Button_OutputType outputId = BUTTON_OUTPUT_MAX;

    if ((ButtonApp_PressedMask & BUTTON_ENTER_MASK) != (uint16)0U)
    {
        outputId = BUTTON_OUTPUT_ENTER;
    }
    else
    {
        /* Both Enter contacts are released. */
    }

    return outputId;
}

/** \brief Resolves the physical direction mask to one logical direction. */
static Button_OutputType ButtonApp_GetDirectionOutput(void)
{
    Button_OutputType outputId;
    uint16 directionMask =
        (uint16)(ButtonApp_PressedMask & BUTTON_DIRECTION_MASK);

    switch (directionMask)
    {
        case BUTTON_DIRECTION_UP_MASK:
            outputId = BUTTON_OUTPUT_DIR_0;
            break;

        case (uint16)(BUTTON_DIRECTION_UP_MASK |
                      BUTTON_DIRECTION_RIGHT_MASK):
            outputId = BUTTON_OUTPUT_DIR_45;
            break;

        case BUTTON_DIRECTION_RIGHT_MASK:
            outputId = BUTTON_OUTPUT_DIR_90;
            break;

        case (uint16)(BUTTON_DIRECTION_DOWN_MASK |
                      BUTTON_DIRECTION_RIGHT_MASK):
            outputId = BUTTON_OUTPUT_DIR_135;
            break;

        case BUTTON_DIRECTION_DOWN_MASK:
            outputId = BUTTON_OUTPUT_DIR_180;
            break;

        case (uint16)(BUTTON_DIRECTION_DOWN_MASK |
                      BUTTON_DIRECTION_LEFT_MASK):
            outputId = BUTTON_OUTPUT_DIR_225;
            break;

        case BUTTON_DIRECTION_LEFT_MASK:
            outputId = BUTTON_OUTPUT_DIR_270;
            break;

        case (uint16)(BUTTON_DIRECTION_UP_MASK |
                      BUTTON_DIRECTION_LEFT_MASK):
            outputId = BUTTON_OUTPUT_DIR_315;
            break;

        default:
            outputId = BUTTON_OUTPUT_MAX;
            break;
    }

    return outputId;
}

/** \brief Clears all report FIFO entries and resets its administration. */
static void ButtonApp_ReportQueueInit(void)
{
    uint8 index;

    ButtonApp_ReportQueueHead = (uint8)0U;
    ButtonApp_ReportQueueTail = (uint8)0U;
    ButtonApp_ReportQueueCount = (uint8)0U;

    for (index = (uint8)0U; index < BUTTON_REPORT_QUEUE_SIZE; index++)
    {
        ButtonApp_ReportQueue[index].outputId = BUTTON_OUTPUT_MAX;
        ButtonApp_ReportQueue[index].ownerId = BUTTON_ID_MAX;
        ButtonApp_ReportQueue[index].signal = BUTTON_SIGNAL_INVALID;
    }
}

/**
 * \brief Appends one validated signal to the report FIFO.
 * \param[in,out] context Source button and owner of the overflow counter.
 * \param[in] signal Signal value to enqueue.
 * \return STD_OK when stored; otherwise STD_NOT_OK.
 */
static Std_ReturnType ButtonApp_ReportQueuePush(
    Button_ContextType * const context,
    Button_OutputType outputId,
    Button_SignalType signal)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((ButtonApp_IsOutputValid(outputId) == TRUE) &&
        (ButtonApp_IsSignalValid(signal) == TRUE) &&
        (ButtonApp_ReportQueueCount < BUTTON_REPORT_QUEUE_SIZE))
    {
        ButtonApp_ReportQueue[ButtonApp_ReportQueueTail].outputId = outputId;
        if ((context != NULL_PTR) &&
            ((uint8)context->id < (uint8)BUTTON_ID_MAX))
        {
            ButtonApp_ReportQueue[ButtonApp_ReportQueueTail].ownerId =
                context->id;
        }
        else
        {
            ButtonApp_ReportQueue[ButtonApp_ReportQueueTail].ownerId =
                BUTTON_ID_MAX;
        }
        ButtonApp_ReportQueue[ButtonApp_ReportQueueTail].signal = signal;

        ButtonApp_ReportQueueTail++;
        if (ButtonApp_ReportQueueTail >= BUTTON_REPORT_QUEUE_SIZE)
        {
            ButtonApp_ReportQueueTail = (uint8)0U;
        }
        else
        {
            /* The report queue tail remains in range. */
        }

        ButtonApp_ReportQueueCount++;
        result = STD_OK;
    }
    else if (ButtonApp_ReportQueueCount >= BUTTON_REPORT_QUEUE_SIZE)
    {
        if ((context != NULL_PTR) &&
            ((uint8)context->id < (uint8)BUTTON_ID_MAX))
        {
            if (context->reportQueueOverflowCount < BUTTON_UINT16_MAX)
            {
                context->reportQueueOverflowCount++;
            }
            else
            {
                /* Keep the per-button overflow counter saturated. */
            }
        }
        else
        {
            if (ButtonApp_GroupReportQueueOverflowCount < BUTTON_UINT16_MAX)
            {
                ButtonApp_GroupReportQueueOverflowCount++;
            }
            else
            {
                /* Keep the grouped-output overflow counter saturated. */
            }
        }
    }
    else
    {
        /* Reject invalid report data. */
    }

    return result;
}

/** \brief Checks whether a logical output identifier is supported. */
static boolean ButtonApp_IsOutputValid(Button_OutputType outputId)
{
    boolean result;

    switch (outputId)
    {
        case BUTTON_OUTPUT_APPS:
        case BUTTON_OUTPUT_BACK:
        case BUTTON_OUTPUT_SEARCH:
        case BUTTON_OUTPUT_ENTER:
        case BUTTON_OUTPUT_DIR_0:
        case BUTTON_OUTPUT_DIR_45:
        case BUTTON_OUTPUT_DIR_90:
        case BUTTON_OUTPUT_DIR_135:
        case BUTTON_OUTPUT_DIR_180:
        case BUTTON_OUTPUT_DIR_225:
        case BUTTON_OUTPUT_DIR_270:
        case BUTTON_OUTPUT_DIR_315:
            result = TRUE;
            break;

        case BUTTON_OUTPUT_MAX:
        default:
            result = FALSE;
            break;
    }

    return result;
}

/** \brief Checks whether a value is defined by Button_SignalType. */
static boolean ButtonApp_IsSignalValid(Button_SignalType signal)
{
    boolean result;

    switch (signal)
    {
        case BUTTON_SIGNAL_CLEAR:
        case BUTTON_SIGNAL_PRESSED:
        case BUTTON_SIGNAL_SHORT_KEY:
        case BUTTON_SIGNAL_LONG_KEY:
        case BUTTON_SIGNAL_INVALID:
            result = TRUE;
            break;

        case BUTTON_SIGNAL_MAX:
        default:
            result = FALSE;
            break;
    }

    return result;
}

/**
 * \brief Copies the oldest queued report without removing it.
 * \param[out] report Destination for the unpacked report.
 * \return TRUE when a report was copied; otherwise FALSE.
 */
static boolean ButtonApp_ReportQueuePeek(Button_ReportType * const report)
{
    boolean result = FALSE;

    if ((report != NULL_PTR) &&
        (ButtonApp_ReportQueueCount > (uint8)0U))
    {
        report->outputId =
            ButtonApp_ReportQueue[ButtonApp_ReportQueueHead].outputId;
        report->ownerId =
            ButtonApp_ReportQueue[ButtonApp_ReportQueueHead].ownerId;
        report->signal =
            ButtonApp_ReportQueue[ButtonApp_ReportQueueHead].signal;
        result = TRUE;
    }
    else
    {
        /* The report queue is empty or the destination is invalid. */
    }

    return result;
}

/** \brief Removes the oldest report when the FIFO is not empty. */
static void ButtonApp_ReportQueuePop(void)
{
    if (ButtonApp_ReportQueueCount > (uint8)0U)
    {
        ButtonApp_ReportQueue[ButtonApp_ReportQueueHead].outputId =
            BUTTON_OUTPUT_MAX;
        ButtonApp_ReportQueue[ButtonApp_ReportQueueHead].ownerId =
            BUTTON_ID_MAX;
        ButtonApp_ReportQueue[ButtonApp_ReportQueueHead].signal =
            BUTTON_SIGNAL_INVALID;

        ButtonApp_ReportQueueHead++;
        if (ButtonApp_ReportQueueHead >= BUTTON_REPORT_QUEUE_SIZE)
        {
            ButtonApp_ReportQueueHead = (uint8)0U;
        }
        else
        {
            /* The report queue head remains in range. */
        }

        ButtonApp_ReportQueueCount--;
    }
    else
    {
        /* Ignore a pop request for an empty queue. */
    }
}

/** \brief Common no-operation entry callback used by button FSM states. */
static void ButtonApp_EntryNoAction(void)
{
    /* Instance-specific entry processing is not required. */
}

/** \brief Common no-operation exit callback used by button FSM states. */
static void ButtonApp_ExitNoAction(void)
{
    /* No exit processing is required. */
}

/** \brief Common no-operation cyclic callback used by button FSM states. */
static void ButtonApp_DoNoAction(void)
{
    /* Debounce timing is managed by the cyclic application function. */
}

/** \brief Unconditionally permits a configured button-state transition. */
static uint8 ButtonApp_GuardAlways(void)
{
    return STD_TRUE;
}

/** \brief No-operation transition action shared by the button FSM table. */
static void ButtonApp_ActionNone(void)
{
    /* Instance-specific effects are handled after dispatch. */
}
