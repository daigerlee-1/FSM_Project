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
    \file   prj_common.c
    \brief  Implements reusable project-independent conversion services.
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

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_common.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Numeric value represented by hexadecimal character 'A' or 'a'. */
#define PRJ_COMMON_HEX_ALPHA_OFFSET ((uint8)10U)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static Std_ReturnType PrjCommon_ConvertHexCharacter(
    uint8 character,
    uint8 * const nibbleValue);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/* No private variables are owned by this module. */

/*******************************************************************************
    9. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Converts four ASCII hexadecimal characters to one 16-bit value. */
Std_ReturnType PrjCommon_ConvertVersion(
    const char * const versionString,
    uint16 * const versionValue)
{
    Std_ReturnType result = STD_NOT_OK;
    uint8 nibbles[PRJ_COMMON_VERSION_HEX_LENGTH];
    uint8 index;
    uint32 convertedValue = (uint32)0U;

    if ((versionString != NULL_PTR) && (versionValue != NULL_PTR))
    {
        result = STD_OK;

        for (index = (uint8)0U;
             index < PRJ_COMMON_VERSION_HEX_LENGTH;
             index++)
        {
            if (PrjCommon_ConvertHexCharacter(
                    (uint8)versionString[index],
                    &nibbles[index]) != STD_OK)
            {
                result = STD_NOT_OK;
                break;
            }
            else
            {
                /* Continue until all four hexadecimal digits are converted. */
            }
        }

        if (result == STD_OK)
        {
            convertedValue =
                ((uint32)nibbles[0] << 12U) |
                ((uint32)nibbles[1] << 8U) |
                ((uint32)nibbles[2] << 4U) |
                (uint32)nibbles[3];
            *versionValue = (uint16)convertedValue;
        }
        else
        {
            /* Preserve the caller's destination after invalid input. */
        }
    }
    else
    {
        /* Reject null source and destination pointers. */
    }

    return result;
}

/*******************************************************************************
    10. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Converts one ASCII hexadecimal character to a 4-bit value. */
static Std_ReturnType PrjCommon_ConvertHexCharacter(
    uint8 character,
    uint8 * const nibbleValue)
{
    Std_ReturnType result = STD_NOT_OK;

    if (nibbleValue != NULL_PTR)
    {
        if ((character >= (uint8)'0') && (character <= (uint8)'9'))
        {
            *nibbleValue = (uint8)(character - (uint8)'0');
            result = STD_OK;
        }
        else if ((character >= (uint8)'A') && (character <= (uint8)'F'))
        {
            *nibbleValue = (uint8)(
                (character - (uint8)'A') + PRJ_COMMON_HEX_ALPHA_OFFSET);
            result = STD_OK;
        }
        else if ((character >= (uint8)'a') && (character <= (uint8)'f'))
        {
            *nibbleValue = (uint8)(
                (character - (uint8)'a') + PRJ_COMMON_HEX_ALPHA_OFFSET);
            result = STD_OK;
        }
        else
        {
            /* Reject characters outside the hexadecimal alphabet. */
        }
    }
    else
    {
        /* Reject a null output pointer. */
    }

    return result;
}
