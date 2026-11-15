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
    \file   lcan_output.h
    \brief  Declares RTE-independent local-CAN output processing interfaces.
            Provides cyclic processing and application-report services used by
            the AUTOSAR SWC integration adapter.
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
| 2026-10-29 000.01     DL      Creation
| 2026-10-29 000.02     DL      Standardized public header documentation
| 2026-10-31 000.03     DL      Added wheel and button CAN output snapshot
| 2026-11-02 000.04     DL      Changed the invalid key value to 0x0F
| 2026-11-05 000.05     DL      Added counted wheel-encoder CAN encoding
| 2026-11-06 000.06     DL      Added logical eight-direction output fields
| 2026-11-06 000.07     DL      Added Encoder and 8-way mutual exclusion
| 2026-11-13 000.08     DL      Added explicit LCAN output initialization
********************************************************************************/
#ifndef LCAN_OUTPUT_H
#define LCAN_OUTPUT_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Released/default CAN key value. */
#define LCAN_OUTPUT_KEY_CLEAR   ((uint8)0U)

/** CAN key value used for an explicitly invalid button report. */
#define LCAN_OUTPUT_KEY_INVALID ((uint8)0x0FU)

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Identifies which wheel input currently owns the CAN output. */
typedef enum
{
    LCAN_WHEEL_INPUT_OWNER_NONE = 0U,
    LCAN_WHEEL_INPUT_OWNER_ENCODER,
    LCAN_WHEEL_INPUT_OWNER_DIRECTION,
    LCAN_WHEEL_INPUT_OWNER_MAX
} LcanOutput_WheelInputOwnerType;

/** Contains values prepared for the cyclic LCAN RTE write runnable. */
typedef struct
{
    uint8 appsKey;
    uint8 backKey;
    uint8 searchKey;
    uint8 enterKey;
    uint8 direction0Degree;
    uint8 direction45Degree;
    uint8 direction90Degree;
    uint8 direction135Degree;
    uint8 direction180Degree;
    uint8 direction225Degree;
    uint8 direction270Degree;
    uint8 direction315Degree;
    /** Encoded movement: CW 0x01-0x7F, CCW 0x80-0xFE. */
    uint8 wheelEncoder;
    boolean wheelEncoderValid;
    /** Input source currently permitted to update wheel-related CAN signals. */
    LcanOutput_WheelInputOwnerType wheelInputOwner;
    /** 10 ms cycles retained after a direction Clear report. */
    uint8 directionReleaseGuardCycles;
    /** Blocks a direction held while Encoder had ownership until its release. */
    boolean directionBlockedUntilClear;
} LcanOutput_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** Initializes all LCAN button and wheel outputs to their specified defaults. */
void LcanOutput_Init(void);

/** Performs cyclic local-CAN output processing at a 10 ms rate. */
void LcanOutput_MainFunction10ms(void);

/** Decodes and stores one packed wheel-direction application report. */
void LcanOutput_ReportWheel(uint8 rawData);

/** Decodes, validates, and stores one packed knob-button report. */
void LcanOutput_ReportButton(uint16 rawData);

/** Returns read-only wheel and button values prepared for RTE transmission. */
const LcanOutput_ContextType * LcanOutput_GetContext(void);

#endif /* LCAN_OUTPUT_H */
