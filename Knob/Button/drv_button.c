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
    \file   drv_button.c
    \brief  Implements validated storage for active-low button inputs.
            The AUTOSAR integration supplies sampled digital levels, while the
            standalone test interface can inject equivalent button states.
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
| 2026-10-26 000.02     DL      Added per-button active-low input handling
| 2026-10-26 000.03     DL      Standardized C source documentation
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "drv_button.h"

/* #include "drv_console.h" */

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/* No private definitions or macros are required. */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are required. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private function prototypes are required. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Latest validated active-low input level for every configured button. */
static uint8 DrvButton_Input[BUTTON_ID_MAX];

/*******************************************************************************
    9. PRIVATE CONSTANTS
*******************************************************************************/
/* No private constants are defined. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes all button input snapshots to the released level. */
void DrvButton_Init(void)
{
    uint8 index;

    for (index = (uint8)0U; index < (uint8)BUTTON_ID_MAX; index++)
    {
        DrvButton_Input[index] = BUTTON_RAW_STATE_RELEASED;
    }
}

/** \brief Reads the active-low snapshot of the default APPS button. */
uint8 DrvButton_Read(void)
{
    return DrvButton_ReadById(BUTTON_ID_APPS);
}

/** \brief Reads one validated per-button input snapshot. */
uint8 DrvButton_ReadById(Button_IdType buttonId)
{
    uint8 input = BUTTON_RAW_STATE_RELEASED;

    if ((uint8)buttonId < (uint8)BUTTON_ID_MAX)
    {
        input = DrvButton_Input[(uint8)buttonId];
    }
    else
    {
        /* Invalid button identifiers are reported as released. */
    }

    return input;
}

/** \brief Validates and stores one active-low button input snapshot. */
Std_ReturnType DrvButton_SetInput(Button_IdType buttonId, uint8 rawState)
{
    Std_ReturnType result = STD_NOT_OK;

    if (((uint8)buttonId < (uint8)BUTTON_ID_MAX) &&
        ((rawState == BUTTON_RAW_STATE_PRESSED) ||
         (rawState == BUTTON_RAW_STATE_RELEASED)))
    {
        DrvButton_Input[(uint8)buttonId] = rawState;
        result = STD_OK;
    }
    else
    {
        /* Reject out-of-range button identifiers and raw input levels. */
    }

    return result;
}

/** \brief Injects a simulated pressed state for the default APPS button. */
void DrvButton_SetSimulatedInput(uint8 isPressed)
{
    DrvButton_SetSimulatedInputById(BUTTON_ID_APPS, isPressed);
}

/** \brief Injects and logs a simulated pressed state for one button. */
void DrvButton_SetSimulatedInputById(Button_IdType buttonId,
                                     uint8 isPressed)
{
    uint8 rawState;

    if (isPressed == STD_TRUE)
    {
        rawState = BUTTON_RAW_STATE_PRESSED;
    }
    else
    {
        rawState = BUTTON_RAW_STATE_RELEASED;
    }

    if (DrvButton_SetInput(buttonId, rawState) == STD_OK)
    {
        if (isPressed == STD_TRUE)
        {
            /* DrvConsole_WriteString("[DRV][BUTTON] raw PRESSED\r\n"); */
        }
        else
        {
            /* DrvConsole_WriteString("[DRV][BUTTON] raw RELEASED\r\n"); */
        }
    }
    else
    {
        /* DrvConsole_WriteString("[DRV][BUTTON] invalid ID\r\n"); */
    }
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/* No private functions are implemented. */
