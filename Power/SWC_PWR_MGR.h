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
    \file   SWC_PWR_MGR.h
    \brief  Declares the public power-management interface for other modules.
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
| 2026-11-12 000.01     DL      Created the public power-management interface
********************************************************************************/
#ifndef SWC_PWR_MGR_H
#define SWC_PWR_MGR_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "power_app.h"
#include "prj_defs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* Public power types are declared by power_app.h. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/

    /** Reports reception of an LCAN message to the power application. */
    Std_ReturnType SWC_PWR_MGR_NotifyCanRx(void);

    /** Supplies one received HU power-state value. */
    Std_ReturnType SWC_PWR_MGR_NotifyHuPowerState(uint8 huPowerState);

    /** Requests the safety-mode power transition. */
    Std_ReturnType SWC_PWR_MGR_RequestSafetyMode(void);

    /** Returns whether the power FSM is in its operational state. */
    boolean SWC_PWR_MGR_IsOperational(void);

    /** Returns the confirmed ACC state after hysteresis processing. */
    boolean SWC_PWR_MGR_IsAccOn(void);

    /** Returns the confirmed battery operating-voltage state. */
    boolean SWC_PWR_MGR_IsBatteryOperationOn(void);

    /** Returns the requested external power-output state. */
    boolean SWC_PWR_MGR_IsOutputEnabled(void);

    /** Returns the latest synchronized power FSM state. */
    Power_StateType SWC_PWR_MGR_GetState(void);

    /** Returns read-only access to the power application context. */
    const Power_ContextType * SWC_PWR_MGR_GetContext(void);


#endif /* SWC_PWR_MGR_H */
