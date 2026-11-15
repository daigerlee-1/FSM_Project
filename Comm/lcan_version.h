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
    \file   lcan_version.h
    \brief  Declares repeated LCAN version-response processing.
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
| 2026-11-10 000.01     DL      Creation
********************************************************************************/
#ifndef LCAN_VERSION_H
#define LCAN_VERSION_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "lcan_input.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No public data types are declared by this module. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Processes SW, CAN DB, and HW version requests every 10 ms.
 *
 * Each newly accepted request generates three successful responses separated
 * by 200 ms. Every response writes 0xFF before the configured Major and Minor
 * values so that AUTOSAR OnChange transmission is triggered repeatedly.
 *
 * \param[in] huSys Latest HU system-request values and validity flags.
 */
void LcanVersion_MainFunction10ms(
    const LcanInput_HuSysType * const huSys);

#endif /* LCAN_VERSION_H */
