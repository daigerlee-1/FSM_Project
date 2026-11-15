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
    \file   SWC_IPdu.c
    \brief  Implements the project-specific AUTOSAR COM TX I-PDU callout.
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
| 2026-10-28 000.01     DL      Creation
| 2026-10-28 000.02     DL      Standardized source documentation
| 2026-11-06 000.03     DL      Added battery-voltage TX permission
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Com_Cbk.h"

#include "SWC_PWR_MGR.h"
#include "prj_typedefs.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/* No private definitions or macros are required. */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private types are required. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are defined. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/* No private variables are declared by this module. */

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private functions are declared by this module. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/**
 * \brief Determines whether the CCP/HU diagnostic TX I-PDU may be transmitted.
 *
 * The callback permits transmission only while the external power output and
 * confirmed battery-voltage range permit CAN transmission. The PDU identifier
 * and payload information are reserved for future project-specific filtering.
 *
 * \param[in] PduId Identifier of the diagnostic TX I-PDU.
 * \param[in] PduInfoPtr Pointer to the I-PDU data and length to be transmitted.
 *
 * \return TRUE when transmission is permitted; otherwise FALSE.
 */
boolean ComIPdu_LCAN_L_CCP_HU_PE_Diag(PduIdType PduId, PduInfoType* PduInfoPtr)
{
    boolean retVal = FALSE;

    (void)PduId;
    (void)PduInfoPtr;

    if ((SWC_PWR_MGR_IsOutputEnabled() == TRUE) &&
        (SWC_PWR_MGR_IsBatteryOperationOn() == TRUE))
    {
        retVal = TRUE;
    }
    else
    {
        /* Suppress I-PDU transmission while the power output is disabled. */
    }

    return retVal;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/* No private functions are implemented by this module. */
