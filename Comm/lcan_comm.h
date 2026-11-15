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
    \file   lcan_comm.h
    \brief  Declares RTE-independent local-CAN communication-mode services.
            The SWC_LCAN RTE adapter implements these services using the ComM
            UserRequest Client/Server interface.
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
********************************************************************************/
#ifndef LCAN_COMM_H
#define LCAN_COMM_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** RTE-independent communication modes supported by the local-CAN channel. */
typedef enum
{
    LCAN_COM_MODE_NO_COM = 0U,
    LCAN_COM_MODE_SILENT_COM,
    LCAN_COM_MODE_FULL_COM,
    LCAN_COM_MODE_INVALID
} LcanCom_ModeType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Reads the communication mode currently applied by ComM.
 * \param[out] comMode Destination for the converted local-CAN mode.
 * \return E_OK when the mode was read and converted; otherwise STD_NOT_OK.
 */
Std_ReturnType LcanCom_GetCurrentMode(LcanCom_ModeType * const comMode);

/**
 * \brief Reads the communication mode most recently requested from ComM.
 * \param[out] comMode Destination for the converted requested mode.
 * \return E_OK when the mode was read and converted; otherwise STD_NOT_OK.
 */
Std_ReturnType LcanCom_GetRequestedMode(LcanCom_ModeType * const comMode);

/**
 * \brief Requests a local-CAN communication mode through ComM.
 * \param[in] comMode Requested RTE-independent local-CAN mode.
 * \return E_OK when ComM accepted the request; otherwise STD_NOT_OK.
 */
Std_ReturnType LcanCom_RequestMode(LcanCom_ModeType comMode);

#endif /* LCAN_COMM_H */
