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
    \file   ccp_context.h
    \brief  Declares the top-level CCP diagnostic context interface.
            The context groups read-only views of each application feature so
            that integration code and test services can inspect system state.
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
| 2026-10-26 000.02     DL      Standardized public header documentation
| 2026-11-13 000.03     DL      Added IVD calculation context observation
********************************************************************************/
#ifndef CCP_CONTEXT_H
#define CCP_CONTEXT_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "SWC_PWR_MGR.h"
#include "button_app.h"
#include "lcan_input.h"
#include "lcan_output.h"
#include "ccp_ivd_check.h"
#include "knob_led.h"
#include "nvm_app.h"
#include "wheel_app.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/**
 * \brief Groups read-only context views for all top-level CCP features.
 */
typedef struct
{
    const Power_ContextType * power; /**< Power-management context. */
    const Led_ContextType * led; /**< Knob LED context. */
    const Button_ContextType * button; /**< Legacy primary-button context. */
    /** Individually indexed button contexts. */
    const Button_ContextType * buttons[BUTTON_ID_MAX];
    const LcanInput_ContextType * canInput; /**< CAN receive context. */
    const LcanOutput_ContextType * canOutput; /**< CAN transmit context. */
    const Nvm_ContextType * nvm; /**< Nonvolatile-storage context. */
    const KnobWheel_ContextType * wheel; /**< Knob-wheel context. */
    const CcpIvdCheck_ContextType * ivd; /**< Integrity-check context. */
} Main_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Returns the read-only top-level CCP application context.
 * \return Pointer to the statically allocated context aggregation object.
 */
const Main_ContextType * Main_GetContext(void);

#endif /* CCP_CONTEXT_H */
