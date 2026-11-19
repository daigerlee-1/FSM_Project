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
    \file   SWC_KnobButton.c
    \brief  Implements the AUTOSAR 10 ms runnable for knob-button inputs.

            The runnable samples the knob-face and wheel-direction button
            inputs through RTE digital-input client ports.
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
| 2026-10-19 000.02     DL      Standardized C source documentation
********************************************************************************/


/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_KnobButton.h"

#include "prj_typedefs.h"

#include "button_app.h"
#include "drv_button.h"

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
/** Indicates that the button application has been initialized. */
static boolean SwcKnobButton_Initialized = FALSE;

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private function prototypes are required. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_KnobButton_START_SEC_CODE
#include "SWC_KnobButton_MemMap.h"

/**
 * \brief Samples all knob and wheel-button digital inputs.
 *
 * This 10 ms runnable requests each configured digital input level through
 * its RTE client port and stores the result in a module-local snapshot.
 */
FUNC(void, SWC_KnobButton_CODE) RE_Knob_Button_Task_10ms(void)
{
    IoHwAb_LevelType inputLevel;
    uint16 reportData;

    if (SwcKnobButton_Initialized == FALSE)
    {
        ButtonApp_Init();
        SwcKnobButton_Initialized = TRUE;
    }
    else
    {
        /* Initialization is performed only once. */
    }

    if (Rte_Call_R_DigDir_BTN_APPS_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_APPS, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_BTN_BACK_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_BACK, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_BTN_ENTER1_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_ENTER1, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_BTN_ENTER2_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_ENTER2, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_BTN_SEARCH_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_SEARCH, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_WHEEL_BTN_DOWN_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_WHEEL_DOWN, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_WHEEL_BTN_LEFT_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_WHEEL_LEFT, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_WHEEL_BTN_RIGHT_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_WHEEL_RIGHT, (uint8)inputLevel);
    }
    if (Rte_Call_R_DigDir_WHEEL_BTN_UP_Input_ReadDirect(&inputLevel) == E_OK)
    {
        (void)DrvButton_SetInput(BUTTON_ID_WHEEL_UP, (uint8)inputLevel);
    }

    ButtonApp_MainFunction10ms();
    if (ButtonApp_GetReport(&reportData) == STD_OK)
    {
        if (Rte_Write_P_u16_Knob_Button_KnobButtonDetected(reportData) == E_OK)
        {
            ButtonApp_ConfirmReport();
        }
        else
        {
            /* Keep the report pending and retry on the next activation. */
        }
    }
    else
    {
        /* No stable button-state change is pending. */
    }
}

#define SWC_KnobButton_STOP_SEC_CODE
#include "SWC_KnobButton_MemMap.h"
