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
    \file   lcan_output.c
    \brief  Implements RTE-independent local-CAN output data processing.
            Wheel-direction and knob-button reports are decoded, validated,
            and retained for CAN output integration.
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
| 2026-10-29 000.01     DL      Separated local-CAN output event processing
| 2026-10-31 000.02     DL      Prepared wheel and button values for RTE output
| 2026-11-02 000.03     DL      Validated the sparse button signal values
| 2026-11-05 000.04     DL      Encoded wheel direction and movement count
| 2026-11-06 000.05     DL      Mapped logical eight-direction output reports
| 2026-11-06 000.06     DL      Added Encoder and 8-way mutual exclusion
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

#include "button_app.h"
#include "lcan_output.h"
#include "wheel_app.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Number of wheel reports retained for debugger inspection. */
#define LCAN_WHEEL_REPORT_LOG_SIZE  ((uint16)512U)

/** Number of button reports retained for debugger inspection. */
#define LCAN_BUTTON_REPORT_LOG_SIZE ((uint16)512U)

/** Reserved wheel-encoder value used when no movement is reported. */
#define LCAN_WHEEL_ENCODER_RESERVED ((uint8)0x00U)

/** Lowest and highest clockwise wheel-encoder movement values. */
#define LCAN_WHEEL_ENCODER_CW_MIN   ((uint8)0x01U)
#define LCAN_WHEEL_ENCODER_CW_MAX   ((uint8)0x7FU)

/** Offset added to a counterclockwise movement count. */
#define LCAN_WHEEL_ENCODER_CCW_BASE ((uint16)0x7FU)

/** Value used when an application wheel report cannot be encoded. */
#define LCAN_WHEEL_ENCODER_INVALID  ((uint8)0xFFU)

/** Direction lock retention after Clear, based on the 10 ms cyclic function. */
#define LCAN_DIRECTION_RELEASE_GUARD_CYCLES ((uint8)3U)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Decoded diagnostic representation of one wheel-direction report. */
typedef struct
{
    uint8 rawData;
    uint8 direction;
    uint8 count;
    uint8 canData;
} LCAN_WheelReportLogType;

/** Decoded diagnostic representation of one knob-button report. */
typedef struct
{
    uint16 rawData;
    uint8 outputId;
    uint8 signal;
    boolean valid;
} LCAN_ButtonReportLogType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are defined. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Wheel and button values consumed by the LCAN output runnable. */
static LcanOutput_ContextType LcanOutput_Context;

/** Trace32-visible circular history of decoded wheel reports. */
static volatile LCAN_WheelReportLogType
    gLcanWheelReportLog[LCAN_WHEEL_REPORT_LOG_SIZE];
/** Next wheel-report history index. */
static volatile uint16 gLcanWheelReportLogIndex;
/** Number of valid wheel-report history entries. */
static volatile uint16 gLcanWheelReportLogCount;
/** Most recently received packed wheel report. */
static volatile uint8 gLcanWheelReportLastRawData;
/** Direction decoded from the most recent wheel report. */
static volatile uint8 gLcanWheelReportLastDirection;
/** Count decoded from the most recent wheel report. */
static volatile uint8 gLcanWheelReportLastCount;
/** CAN value encoded from the most recent wheel report. */
static volatile uint8 gLcanWheelReportLastCanData;

/** Trace32-visible circular history of decoded button reports. */
static volatile LCAN_ButtonReportLogType
    gLcanButtonReportLog[LCAN_BUTTON_REPORT_LOG_SIZE];
/** Next button-report history index. */
static volatile uint16 gLcanButtonReportLogIndex;
/** Number of valid button-report history entries. */
static volatile uint16 gLcanButtonReportLogCount;
/** Most recently received packed button report. */
static volatile uint16 gLcanButtonReportLastRawData;
/** Logical output identifier decoded from the most recent report. */
static volatile uint8 gLcanButtonReportLastOutputId;
/** Signal value decoded from the most recent button report. */
static volatile uint8 gLcanButtonReportLastSignal;
/** Validation result for the most recent button report. */
static volatile boolean gLcanButtonReportLastValid;

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void LCAN_WheelReport_Store(uint8 rawData);
static void LCAN_KnobButtonReport_Store(uint16 rawData);
static boolean LCAN_KnobButtonSignal_IsValid(uint8 signal);
static boolean LCAN_ButtonOutput_IsDirection(uint8 outputId);
static boolean LCAN_DirectionReport_IsAllowed(uint8 signal);
static void LCAN_DirectionOutputs_Clear(void);
static uint8 LCAN_WheelReport_GetDirection(uint8 rawData);
static uint8 LCAN_WheelReport_GetCount(uint8 rawData);
static uint8 LCAN_WheelReport_EncodeCanValue(uint8 direction, uint8 count);

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes every retained LCAN output to its specified default. */
void LcanOutput_Init(void)
{
    LcanOutput_Context.appsKey = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.backKey = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.searchKey = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.enterKey = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction0Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction45Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction90Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction135Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction180Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction225Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction270Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction315Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.wheelEncoder = LCAN_WHEEL_ENCODER_INVALID;
    LcanOutput_Context.wheelEncoderValid = TRUE;
    LcanOutput_Context.wheelInputOwner = LCAN_WHEEL_INPUT_OWNER_NONE;
    LcanOutput_Context.directionReleaseGuardCycles = (uint8)0U;
    LcanOutput_Context.directionBlockedUntilClear = FALSE;
}

void LcanOutput_MainFunction10ms(void)
{
    if ((LcanOutput_Context.wheelInputOwner ==
         LCAN_WHEEL_INPUT_OWNER_DIRECTION) &&
        (LcanOutput_Context.directionReleaseGuardCycles > (uint8)0U))
    {
        LcanOutput_Context.directionReleaseGuardCycles--;
        if (LcanOutput_Context.directionReleaseGuardCycles == (uint8)0U)
        {
            LcanOutput_Context.wheelInputOwner =
                LCAN_WHEEL_INPUT_OWNER_NONE;
        }
        else
        {
            /* Retain direction ownership during the release guard. */
        }
    }
    else
    {
        /* No direction-release guard is active. */
    }
}

/** \brief Decodes and stores one wheel-direction application report. */
void LcanOutput_ReportWheel(uint8 rawData)
{
    LCAN_WheelReport_Store(rawData);
}

/** \brief Decodes and stores one packed knob-button application report. */
void LcanOutput_ReportButton(uint16 rawData)
{
    LCAN_KnobButtonReport_Store(rawData);
}

/** \brief Returns the current wheel and button output snapshot. */
const LcanOutput_ContextType * LcanOutput_GetContext(void)
{
    return &LcanOutput_Context;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Decodes, validates, and stores one packed knob-button report. */
static void LCAN_KnobButtonReport_Store(uint16 rawData)
{
    uint8 outputId;
    uint8 signal;
    boolean valid;
    boolean outputAllowed;
    uint16 writeIndex;

    outputId = (uint8)(rawData >> BUTTON_REPORT_ID_SHIFT);
    signal = (uint8)(rawData & BUTTON_REPORT_SIGNAL_MASK);

    if ((outputId < (uint8)BUTTON_OUTPUT_MAX) &&
        (LCAN_KnobButtonSignal_IsValid(signal) == TRUE))
    {
        valid = TRUE;
    }
    else
    {
        valid = FALSE;
    }

    outputAllowed = valid;
    if ((valid == TRUE) &&
        (LCAN_ButtonOutput_IsDirection(outputId) == TRUE))
    {
        outputAllowed = LCAN_DirectionReport_IsAllowed(signal);
    }
    else
    {
        /* Non-direction button reports do not use wheel-input arbitration. */
    }

    writeIndex = gLcanButtonReportLogIndex;
    if (writeIndex >= LCAN_BUTTON_REPORT_LOG_SIZE)
    {
        writeIndex = (uint16)0U;
    }
    else
    {
        /* The current write index is in range. */
    }

    gLcanButtonReportLog[writeIndex].rawData = rawData;
    gLcanButtonReportLog[writeIndex].outputId = outputId;
    gLcanButtonReportLog[writeIndex].signal = signal;
    gLcanButtonReportLog[writeIndex].valid = valid;

    gLcanButtonReportLastRawData = rawData;
    gLcanButtonReportLastOutputId = outputId;
    gLcanButtonReportLastSignal = signal;
    gLcanButtonReportLastValid = valid;

    if ((valid == TRUE) && (outputAllowed == TRUE))
    {
        switch ((Button_OutputType)outputId)
        {
            case BUTTON_OUTPUT_APPS:
                LcanOutput_Context.appsKey = signal;
                break;

            case BUTTON_OUTPUT_BACK:
                LcanOutput_Context.backKey = signal;
                break;

            case BUTTON_OUTPUT_SEARCH:
                LcanOutput_Context.searchKey = signal;
                break;

            case BUTTON_OUTPUT_ENTER:
                LcanOutput_Context.enterKey = signal;
                break;

            case BUTTON_OUTPUT_DIR_0:
                LcanOutput_Context.direction0Degree = signal;
                break;

            case BUTTON_OUTPUT_DIR_45:
                LcanOutput_Context.direction45Degree = signal;
                break;

            case BUTTON_OUTPUT_DIR_90:
                LcanOutput_Context.direction90Degree = signal;
                break;

            case BUTTON_OUTPUT_DIR_135:
                LcanOutput_Context.direction135Degree = signal;
                break;

            case BUTTON_OUTPUT_DIR_180:
                LcanOutput_Context.direction180Degree = signal;
                break;

            case BUTTON_OUTPUT_DIR_225:
                LcanOutput_Context.direction225Degree = signal;
                break;

            case BUTTON_OUTPUT_DIR_270:
                LcanOutput_Context.direction270Degree = signal;
                break;

            case BUTTON_OUTPUT_DIR_315:
                LcanOutput_Context.direction315Degree = signal;
                break;

            case BUTTON_OUTPUT_MAX:
            default:
                /* Invalid output identifiers do not alter the snapshot. */
                break;
        }
    }
    else
    {
        /* Invalid or mutually excluded reports do not alter the snapshot. */
    }

    writeIndex++;
    if (writeIndex >= LCAN_BUTTON_REPORT_LOG_SIZE)
    {
        writeIndex = (uint16)0U;
    }
    else
    {
        /* The incremented write index remains in range. */
    }
    gLcanButtonReportLogIndex = writeIndex;

    if (gLcanButtonReportLogCount < LCAN_BUTTON_REPORT_LOG_SIZE)
    {
        gLcanButtonReportLogCount++;
    }
    else
    {
        /* Keep the count saturated once the circular log is full. */
    }
}

/** \brief Checks whether a decoded button signal is a defined CAN value. */
static boolean LCAN_KnobButtonSignal_IsValid(uint8 signal)
{
    boolean result;

    switch ((Button_SignalType)signal)
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

/** \brief Checks whether a logical button output is one of eight directions. */
static boolean LCAN_ButtonOutput_IsDirection(uint8 outputId)
{
    boolean result;

    switch ((Button_OutputType)outputId)
    {
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

        case BUTTON_OUTPUT_APPS:
        case BUTTON_OUTPUT_BACK:
        case BUTTON_OUTPUT_SEARCH:
        case BUTTON_OUTPUT_ENTER:
        case BUTTON_OUTPUT_MAX:
        default:
            result = FALSE;
            break;
    }

    return result;
}

/** \brief Applies first-active ownership to one logical direction report. */
static boolean LCAN_DirectionReport_IsAllowed(uint8 signal)
{
    boolean result = FALSE;

    switch ((Button_SignalType)signal)
    {
        case BUTTON_SIGNAL_PRESSED:
            if ((LcanOutput_Context.wheelInputOwner ==
                 LCAN_WHEEL_INPUT_OWNER_NONE) &&
                (LcanOutput_Context.directionBlockedUntilClear == FALSE))
            {
                LcanOutput_Context.wheelInputOwner =
                    LCAN_WHEEL_INPUT_OWNER_DIRECTION;
                LcanOutput_Context.directionReleaseGuardCycles = (uint8)0U;
                LcanOutput_Context.wheelEncoder = LCAN_WHEEL_ENCODER_INVALID;
                LcanOutput_Context.wheelEncoderValid = TRUE;
                result = TRUE;
            }
            else if (LcanOutput_Context.wheelInputOwner ==
                     LCAN_WHEEL_INPUT_OWNER_DIRECTION)
            {
                LcanOutput_Context.directionReleaseGuardCycles = (uint8)0U;
                result = TRUE;
            }
            else
            {
                LcanOutput_Context.directionBlockedUntilClear = TRUE;
            }
            break;

        case BUTTON_SIGNAL_CLEAR:
            LcanOutput_Context.directionBlockedUntilClear = FALSE;
            if (LcanOutput_Context.wheelInputOwner ==
                LCAN_WHEEL_INPUT_OWNER_DIRECTION)
            {
                LcanOutput_Context.directionReleaseGuardCycles =
                    LCAN_DIRECTION_RELEASE_GUARD_CYCLES;
                result = TRUE;
            }
            else
            {
                LCAN_DirectionOutputs_Clear();
            }
            break;

        case BUTTON_SIGNAL_SHORT_KEY:
        case BUTTON_SIGNAL_LONG_KEY:
        case BUTTON_SIGNAL_INVALID:
            if (LcanOutput_Context.wheelInputOwner ==
                LCAN_WHEEL_INPUT_OWNER_DIRECTION)
            {
                result = TRUE;
            }
            else
            {
                /* A different input source currently owns wheel output. */
            }
            break;

        case BUTTON_SIGNAL_MAX:
        default:
            /* Signal validation prevents this branch from being reached. */
            break;
    }

    if (result == TRUE)
    {
        LCAN_DirectionOutputs_Clear();
    }
    else
    {
        /* Suppressed directions remain cleared. */
    }

    return result;
}

/** \brief Forces all eight direction-key outputs to the released value. */
static void LCAN_DirectionOutputs_Clear(void)
{
    LcanOutput_Context.direction0Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction45Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction90Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction135Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction180Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction225Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction270Degree = LCAN_OUTPUT_KEY_CLEAR;
    LcanOutput_Context.direction315Degree = LCAN_OUTPUT_KEY_CLEAR;
}

/** \brief Decodes and stores one packed wheel-direction report. */
static void LCAN_WheelReport_Store(uint8 rawData)
{
    uint8 direction;
    uint8 count;
    uint8 canData;
    uint16 writeIndex;

    direction = LCAN_WheelReport_GetDirection(rawData);
    count = LCAN_WheelReport_GetCount(rawData);
    canData = LCAN_WheelReport_EncodeCanValue(direction, count);
    writeIndex = gLcanWheelReportLogIndex;

    if (writeIndex >= LCAN_WHEEL_REPORT_LOG_SIZE)
    {
        writeIndex = (uint16)0U;
    }
    else
    {
        /* The current write index is in range. */
    }

    gLcanWheelReportLog[writeIndex].rawData = rawData;
    gLcanWheelReportLog[writeIndex].direction = direction;
    gLcanWheelReportLog[writeIndex].count = count;
    gLcanWheelReportLog[writeIndex].canData = canData;

    gLcanWheelReportLastRawData = rawData;
    gLcanWheelReportLastDirection = direction;
    gLcanWheelReportLastCount = count;
    gLcanWheelReportLastCanData = canData;

    if ((canData != LCAN_WHEEL_ENCODER_RESERVED) &&
        (canData != LCAN_WHEEL_ENCODER_INVALID))
    {
        if (LcanOutput_Context.wheelInputOwner ==
            LCAN_WHEEL_INPUT_OWNER_NONE)
        {
            LcanOutput_Context.wheelInputOwner =
                LCAN_WHEEL_INPUT_OWNER_ENCODER;
            LCAN_DirectionOutputs_Clear();
            LcanOutput_Context.wheelEncoder = canData;
        }
        else if (LcanOutput_Context.wheelInputOwner ==
                 LCAN_WHEEL_INPUT_OWNER_ENCODER)
        {
            LcanOutput_Context.wheelEncoder = canData;
        }
        else
        {
            LcanOutput_Context.wheelEncoder = LCAN_WHEEL_ENCODER_INVALID;
        }
        LcanOutput_Context.wheelEncoderValid = TRUE;
    }
    else if (canData == LCAN_WHEEL_ENCODER_INVALID)
    {
        if (LcanOutput_Context.wheelInputOwner ==
            LCAN_WHEEL_INPUT_OWNER_ENCODER)
        {
            LcanOutput_Context.wheelInputOwner =
                LCAN_WHEEL_INPUT_OWNER_NONE;
        }
        else
        {
            /* The Invalid report does not release direction ownership. */
        }
        LcanOutput_Context.wheelEncoder = LCAN_WHEEL_ENCODER_INVALID;
        LcanOutput_Context.wheelEncoderValid = TRUE;
    }
    else
    {
        if (LcanOutput_Context.wheelInputOwner ==
            LCAN_WHEEL_INPUT_OWNER_DIRECTION)
        {
            LcanOutput_Context.wheelEncoder = LCAN_WHEEL_ENCODER_INVALID;
        }
        else
        {
            LcanOutput_Context.wheelEncoder = LCAN_WHEEL_ENCODER_RESERVED;
        }
        LcanOutput_Context.wheelEncoderValid = TRUE;
    }

    writeIndex++;
    if (writeIndex >= LCAN_WHEEL_REPORT_LOG_SIZE)
    {
        writeIndex = (uint16)0U;
    }
    else
    {
        /* The incremented write index remains in range. */
    }
    gLcanWheelReportLogIndex = writeIndex;

    if (gLcanWheelReportLogCount < LCAN_WHEEL_REPORT_LOG_SIZE)
    {
        gLcanWheelReportLogCount++;
    }
    else
    {
        /* Keep the count saturated once the circular log is full. */
    }
}

/** \brief Extracts and validates the direction field of a wheel report. */
static uint8 LCAN_WheelReport_GetDirection(uint8 rawData)
{
    uint8 direction;

    direction = (uint8)(rawData >> KNOB_WHEEL_REPORT_DIRECTION_SHIFT);

    if ((direction != KNOB_WHEEL_DIRECTION_NONE) &&
        (direction != KNOB_WHEEL_DIRECTION_RIGHT) &&
        (direction != KNOB_WHEEL_DIRECTION_LEFT))
    {
        direction = KNOB_WHEEL_DIRECTION_MAX;
    }
    else
    {
        /* The decoded direction is supported. */
    }

    return direction;
}

/** \brief Extracts the direction-count field of a wheel report. */
static uint8 LCAN_WheelReport_GetCount(uint8 rawData)
{
    return (uint8)(rawData & KNOB_WHEEL_REPORT_COUNT_MASK);
}

/** \brief Converts an application direction/count pair to the CAN encoding. */
static uint8 LCAN_WheelReport_EncodeCanValue(uint8 direction, uint8 count)
{
    uint8 result = LCAN_WHEEL_ENCODER_INVALID;
    uint16 encodedValue;

    if ((direction == (uint8)KNOB_WHEEL_DIRECTION_NONE) &&
        (count == (uint8)0U))
    {
        result = LCAN_WHEEL_ENCODER_RESERVED;
    }
    else if ((direction == (uint8)KNOB_WHEEL_DIRECTION_RIGHT) &&
             (count >= LCAN_WHEEL_ENCODER_CW_MIN) &&
             (count <= LCAN_WHEEL_ENCODER_CW_MAX))
    {
        result = count;
    }
    else if ((direction == (uint8)KNOB_WHEEL_DIRECTION_LEFT) &&
             (count >= LCAN_WHEEL_ENCODER_CW_MIN) &&
             (count <= LCAN_WHEEL_ENCODER_CW_MAX))
    {
        encodedValue = LCAN_WHEEL_ENCODER_CCW_BASE + (uint16)count;
        result = (uint8)encodedValue;
    }
    else
    {
        /* Keep the invalid signal value for unsupported combinations. */
    }

    return result;
}

