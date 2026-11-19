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
    \file   prj_common.h
    \brief  Declares reusable project-independent conversion services.
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
| 2026-11-09 000.01     DL      Creation
| 2026-11-10 000.02     DL      Accepted C version strings without pointer casts
********************************************************************************/
#ifndef PRJ_COMMON_H
#define PRJ_COMMON_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Number of ASCII hexadecimal characters consumed by the conversion API. */
#define PRJ_COMMON_VERSION_HEX_LENGTH ((uint8)4U)

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
 * \brief Converts four ASCII hexadecimal characters to one 16-bit value.
 *
 * For example, the byte sequence "26C1" is converted to 0x26C1U. Uppercase
 * and lowercase hexadecimal characters are accepted. The destination remains
 * unchanged if either pointer is null or an input character is invalid.
 *
 * \param[in]  versionString Address of at least four readable bytes.
 * \param[out] versionValue  Destination for the converted value.
 * \return STD_OK on success; otherwise STD_NOT_OK.
 */
Std_ReturnType PrjCommon_ConvertVersion(
    const char * const versionString,
    uint16 * const versionValue);

#endif /* PRJ_COMMON_H */
