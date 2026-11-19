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
    \file   wheel_app.h
    \brief  Declares the event-driven wheel application interface.
            Defines wheel states, events, context data, sensor notifications,
            and direction-report services independently of the AUTOSAR RTE.
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
| 2026-08-26 000.01     DL      Creation
| 2026-10-16 000.02     DL      Refactored to the common event-driven FSM
| 2026-10-26 000.03     DL      Deferred ISR sensor events to the cyclic task
********************************************************************************/
#ifndef WHEEL_APP_H
#define WHEEL_APP_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "fsm.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Maximum number of completed direction reports waiting for transmission. */
#define KNOB_WHEEL_REPORT_QUEUE_SIZE ((uint8)16U)

/** Encoded report layout: upper nibble is direction, lower nibble is count. */
#define KNOB_WHEEL_REPORT_DIRECTION_SHIFT ((uint8)4U)
#define KNOB_WHEEL_REPORT_COUNT_MASK      ((uint8)0x0FU)
#define KNOB_WHEEL_REPORT_COUNT_MAX       ((uint8)0x0FU)

/** Invalid value reported after the movement-information hold period. */
#define KNOB_WHEEL_REPORT_INVALID         ((uint8)0xFFU)

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Identifies the current knob-wheel direction-detection FSM state. */
typedef enum
{
    KNOB_WHEEL_STATE_IDLE = 0U,
    KNOB_WHEEL_STATE_WAIT_INT1,
    KNOB_WHEEL_STATE_WAIT_INT2,
    KNOB_WHEEL_STATE_MAX
} KnobWheel_StateType;

/** Identifies an event accepted by the knob-wheel FSM. */
typedef enum
{
    KNOB_WHEEL_EVENT_INT1 = 0U,
    KNOB_WHEEL_EVENT_INT2,
    KNOB_WHEEL_EVENT_WAIT_TIMEOUT,
    KNOB_WHEEL_EVENT_MAX
} KnobWheel_EventType;

/** Identifies the direction encoded in the RTE report value. */
typedef enum
{
    KNOB_WHEEL_DIRECTION_NONE = 0U,
    KNOB_WHEEL_DIRECTION_RIGHT,
    KNOB_WHEEL_DIRECTION_LEFT,
    KNOB_WHEEL_DIRECTION_MAX
} KnobWheel_DirectionType;

/** Contains the observable state and diagnostics of the knob-wheel module. */
typedef struct
{
    KnobWheel_StateType state;
    KnobWheel_EventType event;
    boolean initialized;
    boolean invalidReported;
    boolean invalidReportPending;
    boolean invalidReportDue;
    boolean countWindowActive;

    uint8 rightCount;
    uint8 leftCount;
    uint8 countOverflowCount;

    uint8 reportQueue[KNOB_WHEEL_REPORT_QUEUE_SIZE];
    uint8 reportQueueHead;
    uint8 reportQueueTail;
    uint8 reportQueueCount;
    uint8 reportQueueOverflowCount;
    uint8 eventQueueOverflowCount;
    uint16 inputQueueOverflowCount;
    boolean reportPending;
    uint8 reportData;

    uint16 int1NotificationCount;
    uint8 int1Photo1Level;
    uint8 int1Photo2Level;

    uint16 int2NotificationCount;
    uint8 int2Photo1Level;
    uint8 int2Photo2Level;

    uint16 rightConfirmedCount;
    uint16 leftConfirmedCount;
    uint16 waitTimeoutCount;
    uint16 rightPatternRejectCount;
    uint16 leftPatternRejectCount;

    Fsm_ContextType fsm;
} KnobWheel_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Initializes the wheel context, FSM, and timers.
 * \return STD_OK when initialization succeeds; otherwise STD_NOT_OK.
 */
Std_ReturnType WheelApp_Init(void);

/** Executes timeout handling and report processing every 10 ms. */
void WheelApp_MainFunction10ms(void);

/** Queues an INT1-edge sensor snapshot from the ICU ISR context. */
Std_ReturnType WheelApp_NotifyPhoto1(uint8 photo1Level, uint8 photo2Level);

/** Queues an INT2-edge sensor snapshot from the ICU ISR context. */
Std_ReturnType WheelApp_NotifyPhoto2(uint8 photo1Level, uint8 photo2Level);

/** Returns the pending wheel-direction report without consuming it. */
Std_ReturnType WheelApp_GetReport(uint8 *reportData);

/** Confirms successful transmission and consumes the pending report. */
void WheelApp_ConfirmReport(void);

/** Returns a read-only diagnostic view of the knob-wheel context. */
const KnobWheel_ContextType * WheelApp_GetContext(void);

#endif /* WHEEL_APP_H */
