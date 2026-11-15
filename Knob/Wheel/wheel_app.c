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
    \file   wheel_app.c
    \brief  Implements the event-driven wheel application state machine.
            Sensor snapshots are converted into FSM events, while the 10 ms
            processing handles timeouts and buffered direction reports.
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
| 2026-10-16 000.02     DL      Refactored to the common event-driven FSM
| 2026-10-26 000.03     DL      Deferred ISR sensor events to the cyclic task
| 2026-11-05 000.04     DL      Held movement data 30 ms before invalid report
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "wheel_app.h"
#include "fsm_timer.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
#define KNOB_WHEEL_LEVEL_LOW          ((uint8)0U)
#define KNOB_WHEEL_LEVEL_HIGH         ((uint8)1U)
#define KNOB_WHEEL_UINT8_MAX          ((uint8)0xFFU)
#define KNOB_WHEEL_UINT16_MAX         ((uint16)0xFFFFU)
#define KNOB_WHEEL_EDGE_TIMEOUT_TICKS ((uint32)30U)
#define KNOB_WHEEL_COUNT_WINDOW_TICKS ((uint32)60U)
#define KNOB_WHEEL_INVALID_DELAY_TICKS ((uint32)30U)
#define KNOB_WHEEL_INPUT_QUEUE_SIZE    ((uint8)16U)

#define KNOB_WHEEL_ENTER_CRITICAL() ((void)0)
#define KNOB_WHEEL_EXIT_CRITICAL()  ((void)0)

#define KNOB_WHEEL_TRANSITION_COUNT                              \
    ((uint16)(sizeof(KnobWheel_Transitions) /                    \
              sizeof(KnobWheel_Transitions[0])))

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Immutable sensor snapshot transferred from an ICU ISR to the 10 ms task. */
typedef struct
{
    KnobWheel_EventType event;
    uint8 photo1Level;
    uint8 photo2Level;
} KnobWheel_InputType;

/** ISR-to-task FIFO preserving photo-sensor edge order and sampled levels. */
typedef struct
{
    KnobWheel_InputType buffer[KNOB_WHEEL_INPUT_QUEUE_SIZE];
    uint8 head;
    uint8 tail;
    uint8 count;
    uint16 overflowCount;
    boolean enabled;
} KnobWheel_InputQueueType;

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void KnobWheel_ContextInit(void);
static void KnobWheel_InputQueueInit(void);
static Std_ReturnType KnobWheel_InputQueuePush(KnobWheel_EventType event,
                                               uint8 photo1Level,
                                               uint8 photo2Level);
static boolean KnobWheel_InputQueuePop(KnobWheel_InputType *input);
static void KnobWheel_ProcessInputQueue(void);
static void KnobWheel_CheckTimerExpired(void);
static void KnobWheel_MainFunction(void);
static void KnobWheel_PostAndDispatch(KnobWheel_EventType event);
static void KnobWheel_UpdateObservableState(void);

static void KnobWheel_EntryIdle(void);
static void KnobWheel_EntryWait(void);
static void KnobWheel_ExitNoAction(void);
static void KnobWheel_DoNoAction(void);
static uint8 KnobWheel_GuardAlways(void);
static void KnobWheel_ActionStartWait(void);
static void KnobWheel_ActionReportRight(void);
static void KnobWheel_ActionReportLeft(void);
static void KnobWheel_ActionCancelWait(void);

static void KnobWheel_ReportDirection(uint8 reportData);
static uint8 KnobWheel_BuildReportData(KnobWheel_DirectionType direction,
                                       uint8 count);
static void KnobWheel_CountDirection(KnobWheel_DirectionType direction);
static void KnobWheel_FlushCountWindow(void);
static void KnobWheel_ReportQueueInit(void);
static boolean KnobWheel_ReportQueuePush(uint8 reportData);
static boolean KnobWheel_ReportQueuePop(uint8 *reportData);
static boolean KnobWheel_IsRightPattern(void);
static boolean KnobWheel_IsLeftPattern(void);
static void KnobWheel_IncrementCounter(uint16 *counter);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Runtime context owned by the knob-wheel application module. */
static KnobWheel_ContextType KnobWheel_Context;

/** Shared FIFO written by ICU ISRs and consumed by the 10 ms task. */
static volatile KnobWheel_InputQueueType KnobWheel_InputQueue;

/*******************************************************************************
    9. PRIVATE CONSTANTS AND FSM CONFIGURATION
*******************************************************************************/
static const Fsm_StateOperationType
    KnobWheel_StateOperations[KNOB_WHEEL_STATE_MAX] =
{
    { KnobWheel_EntryIdle, KnobWheel_ExitNoAction, KnobWheel_DoNoAction },
    { KnobWheel_EntryWait, KnobWheel_ExitNoAction, KnobWheel_DoNoAction },
    { KnobWheel_EntryWait, KnobWheel_ExitNoAction, KnobWheel_DoNoAction }
};

static const Fsm_TransitionType KnobWheel_Transitions[] =
{
    { (Fsm_StateType)KNOB_WHEEL_STATE_IDLE,
      (Fsm_EventType)KNOB_WHEEL_EVENT_INT1,
      KnobWheel_GuardAlways, KnobWheel_ActionStartWait,
      (Fsm_StateType)KNOB_WHEEL_STATE_WAIT_INT2 },
    { (Fsm_StateType)KNOB_WHEEL_STATE_IDLE,
      (Fsm_EventType)KNOB_WHEEL_EVENT_INT2,
      KnobWheel_GuardAlways, KnobWheel_ActionStartWait,
      (Fsm_StateType)KNOB_WHEEL_STATE_WAIT_INT1 },
    { (Fsm_StateType)KNOB_WHEEL_STATE_WAIT_INT2,
      (Fsm_EventType)KNOB_WHEEL_EVENT_INT2,
      KnobWheel_GuardAlways, KnobWheel_ActionReportRight,
      (Fsm_StateType)KNOB_WHEEL_STATE_IDLE },
    { (Fsm_StateType)KNOB_WHEEL_STATE_WAIT_INT2,
      (Fsm_EventType)KNOB_WHEEL_EVENT_WAIT_TIMEOUT,
      KnobWheel_GuardAlways, KnobWheel_ActionCancelWait,
      (Fsm_StateType)KNOB_WHEEL_STATE_IDLE },
    { (Fsm_StateType)KNOB_WHEEL_STATE_WAIT_INT1,
      (Fsm_EventType)KNOB_WHEEL_EVENT_INT1,
      KnobWheel_GuardAlways, KnobWheel_ActionReportLeft,
      (Fsm_StateType)KNOB_WHEEL_STATE_IDLE },
    { (Fsm_StateType)KNOB_WHEEL_STATE_WAIT_INT1,
      (Fsm_EventType)KNOB_WHEEL_EVENT_WAIT_TIMEOUT,
      KnobWheel_GuardAlways, KnobWheel_ActionCancelWait,
      (Fsm_StateType)KNOB_WHEEL_STATE_IDLE }
};

static const Fsm_ConfigType KnobWheel_FsmConfig =
{
    (Fsm_StateType)KNOB_WHEEL_STATE_IDLE,
    (Fsm_StateType)KNOB_WHEEL_STATE_MAX,
    (Fsm_EventType)KNOB_WHEEL_EVENT_MAX,
    KnobWheel_StateOperations,
    KnobWheel_Transitions,
    KNOB_WHEEL_TRANSITION_COUNT
};

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes the wheel context, FSM, and timers. */
Std_ReturnType WheelApp_Init(void)
{
    Std_ReturnType fsmResult;

    KnobWheel_InputQueueInit();
    KnobWheel_ContextInit();
    FSM_Timer_Stop(FSM_TIMER_KNOB_WHEEL);
    FSM_Timer_Stop(FSM_TIMER_KNOB_WHEEL_COUNT_WINDOW);
    FSM_Timer_Stop(FSM_TIMER_KNOB_WHEEL_INVALID_DELAY);

    fsmResult = Fsm_Init(&KnobWheel_Context.fsm, &KnobWheel_FsmConfig);
    if (fsmResult == STD_OK)
    {
        KnobWheel_Context.initialized = TRUE;
        KnobWheel_UpdateObservableState();

        KNOB_WHEEL_ENTER_CRITICAL();
        KnobWheel_InputQueue.enabled = TRUE;
        KNOB_WHEEL_EXIT_CRITICAL();
    }
    else
    {
        KnobWheel_Context.initialized = FALSE;
    }

    return fsmResult;
}

/** \brief Executes timeout checks and sends at most one queued report. */
void WheelApp_MainFunction10ms(void)
{
    if (KnobWheel_Context.initialized == TRUE)
    {
        KnobWheel_ProcessInputQueue();
        KnobWheel_CheckTimerExpired();
        Fsm_Dispatch(&KnobWheel_Context.fsm);
        Fsm_Do(&KnobWheel_Context.fsm);
        KnobWheel_UpdateObservableState();
        KnobWheel_MainFunction();
    }
    else
    {
        /* Initialization is required before cyclic processing. */
    }
}

/** \brief Returns a read-only diagnostic view of the knob-wheel context. */
const KnobWheel_ContextType * WheelApp_GetContext(void)
{
    return &KnobWheel_Context;
}

/** \brief Queues an INT1 sensor snapshot without dispatching from the ISR. */
Std_ReturnType WheelApp_NotifyPhoto1(uint8 photo1Level, uint8 photo2Level)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((photo1Level <= KNOB_WHEEL_LEVEL_HIGH) &&
        (photo2Level <= KNOB_WHEEL_LEVEL_HIGH))
    {
        result = KnobWheel_InputQueuePush(KNOB_WHEEL_EVENT_INT1,
                                          photo1Level,
                                          photo2Level);
    }
    else
    {
        /* Reject invalid sensor levels and premature notifications. */
    }

    return result;
}

/** \brief Queues an INT2 sensor snapshot without dispatching from the ISR. */
Std_ReturnType WheelApp_NotifyPhoto2(uint8 photo1Level, uint8 photo2Level)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((photo1Level <= KNOB_WHEEL_LEVEL_HIGH) &&
        (photo2Level <= KNOB_WHEEL_LEVEL_HIGH))
    {
        result = KnobWheel_InputQueuePush(KNOB_WHEEL_EVENT_INT2,
                                          photo1Level,
                                          photo2Level);
    }
    else
    {
        /* Reject invalid sensor levels and premature notifications. */
    }

    return result;
}

/** \brief Returns one pending direction report without consuming it. */
Std_ReturnType WheelApp_GetReport(uint8 *reportData)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((reportData != NULL_PTR) &&
        (KnobWheel_Context.reportPending == TRUE))
    {
        *reportData = KnobWheel_Context.reportData;
        result = STD_OK;
    }
    else
    {
        /* No report is pending or the destination pointer is invalid. */
    }

    return result;
}

/** \brief Consumes a direction report after successful transmission. */
void WheelApp_ConfirmReport(void)
{
    KnobWheel_Context.reportPending = FALSE;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
static void KnobWheel_ContextInit(void)
{
    KnobWheel_Context.state = KNOB_WHEEL_STATE_IDLE;
    KnobWheel_Context.event = KNOB_WHEEL_EVENT_MAX;
    KnobWheel_Context.initialized = FALSE;
    KnobWheel_Context.invalidReported = TRUE;
    KnobWheel_Context.invalidReportPending = FALSE;
    KnobWheel_Context.invalidReportDue = FALSE;
    KnobWheel_Context.countWindowActive = FALSE;
    KnobWheel_Context.rightCount = (uint8)0U;
    KnobWheel_Context.leftCount = (uint8)0U;
    KnobWheel_Context.countOverflowCount = (uint8)0U;
    KnobWheel_Context.reportQueueOverflowCount = (uint8)0U;
    KnobWheel_Context.eventQueueOverflowCount = (uint8)0U;
    KnobWheel_Context.inputQueueOverflowCount = (uint16)0U;
    KnobWheel_Context.reportPending = FALSE;
    KnobWheel_Context.reportData =
        KnobWheel_BuildReportData(KNOB_WHEEL_DIRECTION_NONE, (uint8)0U);
    KnobWheel_Context.int1NotificationCount = (uint16)0U;
    KnobWheel_Context.int1Photo1Level = KNOB_WHEEL_LEVEL_LOW;
    KnobWheel_Context.int1Photo2Level = KNOB_WHEEL_LEVEL_LOW;
    KnobWheel_Context.int2NotificationCount = (uint16)0U;
    KnobWheel_Context.int2Photo1Level = KNOB_WHEEL_LEVEL_LOW;
    KnobWheel_Context.int2Photo2Level = KNOB_WHEEL_LEVEL_LOW;
    KnobWheel_Context.rightConfirmedCount = (uint16)0U;
    KnobWheel_Context.leftConfirmedCount = (uint16)0U;
    KnobWheel_Context.waitTimeoutCount = (uint16)0U;
    KnobWheel_Context.rightPatternRejectCount = (uint16)0U;
    KnobWheel_Context.leftPatternRejectCount = (uint16)0U;
    KnobWheel_ReportQueueInit();
}

/** \brief Clears and disables the ISR-to-task photo-sensor input FIFO. */
static void KnobWheel_InputQueueInit(void)
{
    uint8 index;

    KNOB_WHEEL_ENTER_CRITICAL();

    KnobWheel_InputQueue.enabled = FALSE;
    KnobWheel_InputQueue.head = (uint8)0U;
    KnobWheel_InputQueue.tail = (uint8)0U;
    KnobWheel_InputQueue.count = (uint8)0U;
    KnobWheel_InputQueue.overflowCount = (uint16)0U;

    for (index = (uint8)0U;
         index < KNOB_WHEEL_INPUT_QUEUE_SIZE;
         index++)
    {
        KnobWheel_InputQueue.buffer[index].event = KNOB_WHEEL_EVENT_MAX;
        KnobWheel_InputQueue.buffer[index].photo1Level = KNOB_WHEEL_LEVEL_LOW;
        KnobWheel_InputQueue.buffer[index].photo2Level = KNOB_WHEEL_LEVEL_LOW;
    }

    KNOB_WHEEL_EXIT_CRITICAL();
}

/** \brief Adds one sensor snapshot to the shared FIFO from an ICU ISR. */
static Std_ReturnType KnobWheel_InputQueuePush(KnobWheel_EventType event,
                                               uint8 photo1Level,
                                               uint8 photo2Level)
{
    Std_ReturnType result = STD_NOT_OK;
    uint8 tail;

    KNOB_WHEEL_ENTER_CRITICAL();

    if ((KnobWheel_InputQueue.enabled == TRUE) &&
        (KnobWheel_InputQueue.count < KNOB_WHEEL_INPUT_QUEUE_SIZE))
    {
        tail = KnobWheel_InputQueue.tail;
        KnobWheel_InputQueue.buffer[tail].event = event;
        KnobWheel_InputQueue.buffer[tail].photo1Level = photo1Level;
        KnobWheel_InputQueue.buffer[tail].photo2Level = photo2Level;

        tail++;
        if (tail >= KNOB_WHEEL_INPUT_QUEUE_SIZE)
        {
            tail = (uint8)0U;
        }
        else
        {
            /* The queue tail remains within the storage array. */
        }

        KnobWheel_InputQueue.tail = tail;
        KnobWheel_InputQueue.count++;
        result = STD_OK;
    }
    else if (KnobWheel_InputQueue.enabled == TRUE)
    {
        if (KnobWheel_InputQueue.overflowCount < KNOB_WHEEL_UINT16_MAX)
        {
            KnobWheel_InputQueue.overflowCount++;
        }
        else
        {
            /* Keep the ISR-side overflow counter saturated. */
        }
    }
    else
    {
        /* Reject notifications until wheel initialization has completed. */
    }

    KNOB_WHEEL_EXIT_CRITICAL();

    return result;
}

/** \brief Removes one ordered sensor snapshot for 10 ms task processing. */
static boolean KnobWheel_InputQueuePop(KnobWheel_InputType *input)
{
    boolean result = FALSE;
    uint8 head;

    if (input != NULL_PTR)
    {
        KNOB_WHEEL_ENTER_CRITICAL();

        if (KnobWheel_InputQueue.count > (uint8)0U)
        {
            head = KnobWheel_InputQueue.head;
            input->event = KnobWheel_InputQueue.buffer[head].event;
            input->photo1Level =
                KnobWheel_InputQueue.buffer[head].photo1Level;
            input->photo2Level =
                KnobWheel_InputQueue.buffer[head].photo2Level;

            KnobWheel_InputQueue.buffer[head].event = KNOB_WHEEL_EVENT_MAX;
            KnobWheel_InputQueue.buffer[head].photo1Level =
                KNOB_WHEEL_LEVEL_LOW;
            KnobWheel_InputQueue.buffer[head].photo2Level =
                KNOB_WHEEL_LEVEL_LOW;

            head++;
            if (head >= KNOB_WHEEL_INPUT_QUEUE_SIZE)
            {
                head = (uint8)0U;
            }
            else
            {
                /* The queue head remains within the storage array. */
            }

            KnobWheel_InputQueue.head = head;
            KnobWheel_InputQueue.count--;
            result = TRUE;
        }
        else
        {
            /* The ISR-to-task queue is empty. */
        }

        KNOB_WHEEL_EXIT_CRITICAL();
    }
    else
    {
        /* Invalid output pointers are rejected. */
    }

    return result;
}

/** \brief Processes queued ISR snapshots exclusively in the 10 ms task. */
static void KnobWheel_ProcessInputQueue(void)
{
    KnobWheel_InputType input;
    uint16 overflowCount;
    uint8 processedCount;

    KNOB_WHEEL_ENTER_CRITICAL();
    overflowCount = KnobWheel_InputQueue.overflowCount;
    KNOB_WHEEL_EXIT_CRITICAL();
    KnobWheel_Context.inputQueueOverflowCount = overflowCount;

    for (processedCount = (uint8)0U;
         processedCount < KNOB_WHEEL_INPUT_QUEUE_SIZE;
         processedCount++)
    {
        if (KnobWheel_InputQueuePop(&input) == FALSE)
        {
            break;
        }
        else if (input.event == KNOB_WHEEL_EVENT_INT1)
        {
            KnobWheel_IncrementCounter(
                &KnobWheel_Context.int1NotificationCount);
            KnobWheel_Context.int1Photo1Level = input.photo1Level;
            KnobWheel_Context.int1Photo2Level = input.photo2Level;
            KnobWheel_PostAndDispatch(KNOB_WHEEL_EVENT_INT1);
        }
        else if (input.event == KNOB_WHEEL_EVENT_INT2)
        {
            KnobWheel_IncrementCounter(
                &KnobWheel_Context.int2NotificationCount);
            KnobWheel_Context.int2Photo1Level = input.photo1Level;
            KnobWheel_Context.int2Photo2Level = input.photo2Level;
            KnobWheel_PostAndDispatch(KNOB_WHEEL_EVENT_INT2);
        }
        else
        {
            /* Ignore a defensively invalid queued event. */
        }
    }
}

static void KnobWheel_CheckTimerExpired(void)
{
    if (FSM_Timer_IsExpired(FSM_TIMER_KNOB_WHEEL) == TRUE)
    {
        (void)Fsm_PostEvent(&KnobWheel_Context.fsm,
                            (Fsm_EventType)KNOB_WHEEL_EVENT_WAIT_TIMEOUT);
    }
    else
    {
        /* The edge-pair timeout has not expired. */
    }

    if (FSM_Timer_IsExpired(FSM_TIMER_KNOB_WHEEL_COUNT_WINDOW) == TRUE)
    {
        KnobWheel_FlushCountWindow();
    }
    else
    {
        /* The direction-counting window remains active or stopped. */
    }

    if (FSM_Timer_IsExpired(FSM_TIMER_KNOB_WHEEL_INVALID_DELAY) == TRUE)
    {
        KnobWheel_Context.invalidReportDue = TRUE;
    }
    else
    {
        /* The movement-information hold period has not expired. */
    }
}

static void KnobWheel_MainFunction(void)
{
    uint8 reportData;

    if (KnobWheel_Context.reportPending == TRUE)
    {
        /* Wait until the integration layer consumes the previous report. */
    }
    else if (KnobWheel_Context.invalidReportDue == TRUE)
    {
        KnobWheel_ReportDirection(KNOB_WHEEL_REPORT_INVALID);
        KnobWheel_Context.invalidReportPending = FALSE;
        KnobWheel_Context.invalidReportDue = FALSE;
        KnobWheel_Context.invalidReported = TRUE;
    }
    else if (KnobWheel_Context.invalidReportPending == TRUE)
    {
        /* Hold the movement report until its 30 ms period expires. */
    }
    else if (KnobWheel_ReportQueuePop(&reportData) == TRUE)
    {
        KnobWheel_ReportDirection(reportData);
        KnobWheel_Context.invalidReported = FALSE;
        KnobWheel_Context.invalidReportPending = TRUE;
        FSM_Timer_Start(FSM_TIMER_KNOB_WHEEL_INVALID_DELAY,
                        KNOB_WHEEL_INVALID_DELAY_TICKS);
    }
    else
    {
        /* Keep the previously prepared direction value. */
    }
}

static void KnobWheel_PostAndDispatch(KnobWheel_EventType event)
{
    if (Fsm_PostEvent(&KnobWheel_Context.fsm,
                      (Fsm_EventType)event) == STD_OK)
    {
        Fsm_Dispatch(&KnobWheel_Context.fsm);
        KnobWheel_UpdateObservableState();
    }
    else if (KnobWheel_Context.eventQueueOverflowCount < KNOB_WHEEL_UINT8_MAX)
    {
        KnobWheel_Context.eventQueueOverflowCount++;
    }
    else
    {
        /* Keep the diagnostic overflow counter saturated. */
    }
}

static void KnobWheel_UpdateObservableState(void)
{
    KnobWheel_Context.state =
        (KnobWheel_StateType)Fsm_GetState(&KnobWheel_Context.fsm);
    KnobWheel_Context.event =
        (KnobWheel_EventType)Fsm_GetLastEvent(&KnobWheel_Context.fsm);
}

static void KnobWheel_EntryIdle(void)
{
    /* Direction detection is ready for a new first edge. */
}

static void KnobWheel_EntryWait(void)
{
    /* The matching second edge is awaited until the timer expires. */
}

static void KnobWheel_ExitNoAction(void)
{
    /* No state-specific exit operation is required. */
}

static void KnobWheel_DoNoAction(void)
{
    /* Cyclic work is handled by WheelApp_MainFunction10ms(). */
}

static uint8 KnobWheel_GuardAlways(void)
{
    return STD_TRUE;
}

static void KnobWheel_ActionStartWait(void)
{
    FSM_Timer_Start(FSM_TIMER_KNOB_WHEEL,
                    KNOB_WHEEL_EDGE_TIMEOUT_TICKS);
}

static void KnobWheel_ActionReportRight(void)
{
    FSM_Timer_Stop(FSM_TIMER_KNOB_WHEEL);
    if (KnobWheel_IsRightPattern() == TRUE)
    {
        KnobWheel_IncrementCounter(&KnobWheel_Context.rightConfirmedCount);
        KnobWheel_CountDirection(KNOB_WHEEL_DIRECTION_RIGHT);
    }
    else
    {
        KnobWheel_IncrementCounter(&KnobWheel_Context.rightPatternRejectCount);
    }
}

static void KnobWheel_ActionReportLeft(void)
{
    FSM_Timer_Stop(FSM_TIMER_KNOB_WHEEL);
    if (KnobWheel_IsLeftPattern() == TRUE)
    {
        KnobWheel_IncrementCounter(&KnobWheel_Context.leftConfirmedCount);
        KnobWheel_CountDirection(KNOB_WHEEL_DIRECTION_LEFT);
    }
    else
    {
        KnobWheel_IncrementCounter(&KnobWheel_Context.leftPatternRejectCount);
    }
}

static void KnobWheel_ActionCancelWait(void)
{
    KnobWheel_IncrementCounter(&KnobWheel_Context.waitTimeoutCount);
    FSM_Timer_Stop(FSM_TIMER_KNOB_WHEEL);
}

static void KnobWheel_ReportDirection(uint8 reportData)
{
    KnobWheel_Context.reportData = reportData;
    KnobWheel_Context.reportPending = TRUE;
}

static uint8 KnobWheel_BuildReportData(KnobWheel_DirectionType direction,
                                       uint8 count)
{
    KnobWheel_DirectionType safeDirection = direction;
    uint8 safeCount = count;
    uint16 shiftedDirection;

    if (safeDirection >= KNOB_WHEEL_DIRECTION_MAX)
    {
        safeDirection = KNOB_WHEEL_DIRECTION_NONE;
    }
    else
    {
        /* The requested direction is valid. */
    }

    if (safeCount > KNOB_WHEEL_REPORT_COUNT_MAX)
    {
        safeCount = KNOB_WHEEL_REPORT_COUNT_MAX;
    }
    else
    {
        /* The requested count fits in the report field. */
    }

    shiftedDirection =
        (uint16)((uint16)safeDirection <<
                 KNOB_WHEEL_REPORT_DIRECTION_SHIFT);

    return (uint8)((uint8)shiftedDirection |
                   (uint8)(safeCount & KNOB_WHEEL_REPORT_COUNT_MASK));
}

static void KnobWheel_CountDirection(KnobWheel_DirectionType direction)
{
    if (KnobWheel_Context.countWindowActive == FALSE)
    {
        KnobWheel_Context.countWindowActive = TRUE;
        KnobWheel_Context.rightCount = (uint8)0U;
        KnobWheel_Context.leftCount = (uint8)0U;
        FSM_Timer_Start(FSM_TIMER_KNOB_WHEEL_COUNT_WINDOW,
                        KNOB_WHEEL_COUNT_WINDOW_TICKS);
    }
    else
    {
        /* Continue accumulating within the active window. */
    }

    if (direction == KNOB_WHEEL_DIRECTION_RIGHT)
    {
        if (KnobWheel_Context.rightCount < KNOB_WHEEL_REPORT_COUNT_MAX)
        {
            KnobWheel_Context.rightCount++;
        }
        else if (KnobWheel_Context.countOverflowCount < KNOB_WHEEL_UINT8_MAX)
        {
            KnobWheel_Context.countOverflowCount++;
        }
        else
        {
            /* Keep the diagnostic overflow counter saturated. */
        }
    }
    else if (direction == KNOB_WHEEL_DIRECTION_LEFT)
    {
        if (KnobWheel_Context.leftCount < KNOB_WHEEL_REPORT_COUNT_MAX)
        {
            KnobWheel_Context.leftCount++;
        }
        else if (KnobWheel_Context.countOverflowCount < KNOB_WHEEL_UINT8_MAX)
        {
            KnobWheel_Context.countOverflowCount++;
        }
        else
        {
            /* Keep the diagnostic overflow counter saturated. */
        }
    }
    else
    {
        /* NONE and invalid directions are not counted. */
    }
}

static void KnobWheel_FlushCountWindow(void)
{
    if (KnobWheel_Context.countWindowActive == TRUE)
    {
        FSM_Timer_Stop(FSM_TIMER_KNOB_WHEEL_COUNT_WINDOW);

        if (KnobWheel_Context.rightCount > (uint8)0U)
        {
            (void)KnobWheel_ReportQueuePush(
                KnobWheel_BuildReportData(KNOB_WHEEL_DIRECTION_RIGHT,
                                          KnobWheel_Context.rightCount));
        }
        else
        {
            /* No right-direction movement was counted. */
        }

        if (KnobWheel_Context.leftCount > (uint8)0U)
        {
            (void)KnobWheel_ReportQueuePush(
                KnobWheel_BuildReportData(KNOB_WHEEL_DIRECTION_LEFT,
                                          KnobWheel_Context.leftCount));
        }
        else
        {
            /* No left-direction movement was counted. */
        }

        KnobWheel_Context.rightCount = (uint8)0U;
        KnobWheel_Context.leftCount = (uint8)0U;
        KnobWheel_Context.countWindowActive = FALSE;
    }
    else
    {
        /* Ignore a stale count-window expiration. */
    }
}

static void KnobWheel_ReportQueueInit(void)
{
    uint8 index;

    for (index = (uint8)0U;
         index < KNOB_WHEEL_REPORT_QUEUE_SIZE;
         index++)
    {
        KnobWheel_Context.reportQueue[index] =
            KnobWheel_BuildReportData(KNOB_WHEEL_DIRECTION_NONE,
                                      (uint8)0U);
    }

    KnobWheel_Context.reportQueueHead = (uint8)0U;
    KnobWheel_Context.reportQueueTail = (uint8)0U;
    KnobWheel_Context.reportQueueCount = (uint8)0U;
}

static boolean KnobWheel_ReportQueuePush(uint8 reportData)
{
    boolean result = FALSE;

    if (KnobWheel_Context.reportQueueCount < KNOB_WHEEL_REPORT_QUEUE_SIZE)
    {
        KnobWheel_Context.reportQueue[KnobWheel_Context.reportQueueTail] =
            reportData;
        KnobWheel_Context.reportQueueTail++;
        if (KnobWheel_Context.reportQueueTail >= KNOB_WHEEL_REPORT_QUEUE_SIZE)
        {
            KnobWheel_Context.reportQueueTail = (uint8)0U;
        }
        else
        {
            /* The queue tail remains within the storage array. */
        }

        KnobWheel_Context.reportQueueCount++;
        result = TRUE;
    }
    else if (KnobWheel_Context.reportQueueOverflowCount < KNOB_WHEEL_UINT8_MAX)
    {
        KnobWheel_Context.reportQueueOverflowCount++;
    }
    else
    {
        /* Keep the diagnostic overflow counter saturated. */
    }

    return result;
}

static boolean KnobWheel_ReportQueuePop(uint8 *reportData)
{
    boolean result = FALSE;

    if ((reportData != NULL_PTR) &&
        (KnobWheel_Context.reportQueueCount > (uint8)0U))
    {
        *reportData =
            KnobWheel_Context.reportQueue[KnobWheel_Context.reportQueueHead];
        KnobWheel_Context.reportQueue[KnobWheel_Context.reportQueueHead] =
            KnobWheel_BuildReportData(KNOB_WHEEL_DIRECTION_NONE,
                                      (uint8)0U);
        KnobWheel_Context.reportQueueHead++;
        if (KnobWheel_Context.reportQueueHead >= KNOB_WHEEL_REPORT_QUEUE_SIZE)
        {
            KnobWheel_Context.reportQueueHead = (uint8)0U;
        }
        else
        {
            /* The queue head remains within the storage array. */
        }

        KnobWheel_Context.reportQueueCount--;
        result = TRUE;
    }
    else
    {
        /* The queue is empty or the output pointer is invalid. */
    }

    return result;
}

static boolean KnobWheel_IsRightPattern(void)
{
    boolean result = FALSE;

    if (((KnobWheel_Context.int1Photo1Level == KNOB_WHEEL_LEVEL_HIGH) &&
         (KnobWheel_Context.int1Photo2Level == KNOB_WHEEL_LEVEL_LOW) &&
         (KnobWheel_Context.int2Photo1Level == KNOB_WHEEL_LEVEL_HIGH) &&
         (KnobWheel_Context.int2Photo2Level == KNOB_WHEEL_LEVEL_HIGH)) ||
        ((KnobWheel_Context.int1Photo1Level == KNOB_WHEEL_LEVEL_LOW) &&
         (KnobWheel_Context.int1Photo2Level == KNOB_WHEEL_LEVEL_HIGH) &&
         (KnobWheel_Context.int2Photo1Level == KNOB_WHEEL_LEVEL_LOW) &&
         (KnobWheel_Context.int2Photo2Level == KNOB_WHEEL_LEVEL_LOW)))
    {
        result = TRUE;
    }
    else
    {
        /* The captured levels do not form a valid right-turn pattern. */
    }

    return result;
}

static boolean KnobWheel_IsLeftPattern(void)
{
    boolean result = FALSE;

    if (((KnobWheel_Context.int1Photo1Level == KNOB_WHEEL_LEVEL_HIGH) &&
         (KnobWheel_Context.int1Photo2Level == KNOB_WHEEL_LEVEL_HIGH) &&
         (KnobWheel_Context.int2Photo1Level == KNOB_WHEEL_LEVEL_LOW) &&
         (KnobWheel_Context.int2Photo2Level == KNOB_WHEEL_LEVEL_HIGH)) ||
        ((KnobWheel_Context.int1Photo1Level == KNOB_WHEEL_LEVEL_LOW) &&
         (KnobWheel_Context.int1Photo2Level == KNOB_WHEEL_LEVEL_LOW) &&
         (KnobWheel_Context.int2Photo1Level == KNOB_WHEEL_LEVEL_HIGH) &&
         (KnobWheel_Context.int2Photo2Level == KNOB_WHEEL_LEVEL_LOW)))
    {
        result = TRUE;
    }
    else
    {
        /* The captured levels do not form a valid left-turn pattern. */
    }

    return result;
}

static void KnobWheel_IncrementCounter(uint16 *counter)
{
    if (counter != NULL_PTR)
    {
        if (*counter < KNOB_WHEEL_UINT16_MAX)
        {
            (*counter)++;
        }
        else
        {
            /* Keep the diagnostic counter saturated. */
        }
    }
    else
    {
        /* Invalid counter pointers are ignored. */
    }
}

