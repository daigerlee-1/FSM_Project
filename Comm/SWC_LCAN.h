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
    \file   SWC_LCAN.h
    \brief  Declares the public LCAN service interface used by other modules.
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
| 2026-11-12 000.01     DL      Created the public LCAN service interface
| 2026-11-12 000.02     DL      Added the combined LCAN observation context
********************************************************************************/
#ifndef SWC_LCAN_H
#define SWC_LCAN_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "lcan_input.h"
#include "lcan_output.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Groups read-only LCAN input and output contexts for runtime observation. */
typedef struct
{
    /** Latest validated LCAN receive-message snapshots. */
    const LcanInput_ContextType * input;

    /** Latest button, wheel, and diagnostic output state. */
    const LcanOutput_ContextType * output;
} SWC_LCAN_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Returns the selected vehicle powertrain start/ready condition.
 *
 * The configured ICE, HEV, or EV input signal is evaluated by the LCAN input
 * module. An invalid or timed-out signal is reported as FALSE.
 *
 * \return TRUE when the selected valid signal is active; otherwise FALSE.
 */
boolean SWC_LCAN_IsEngineStarted(void);

/**
 * \brief Returns the combined read-only LCAN observation context.
 * \return Pointer to the module-owned input/output context aggregation.
 */
const SWC_LCAN_ContextType * SWC_LCAN_GetContext(void);

#endif /* SWC_LCAN_H */
