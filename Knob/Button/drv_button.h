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
    \file   drv_button.h
    \brief  Declares the button-input abstraction used by the application.
            Provides active-low raw input storage, validated per-button access,
            and standalone-host simulation services.
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
| 2026-10-26 000.03     DL      Standardized public header documentation
********************************************************************************/
#ifndef DRV_BUTTON_H
#define DRV_BUTTON_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Active-low raw input level indicating that a button is pressed. */
#define BUTTON_RAW_STATE_PRESSED  ((uint8)0U)

/** Active-low raw input level indicating that a button is released. */
#define BUTTON_RAW_STATE_RELEASED ((uint8)1U)

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Identifies one physical knob or wheel button input. */
typedef enum
{
    BUTTON_ID_APPS = 0U,
    BUTTON_ID_BACK,
    BUTTON_ID_ENTER1,
    BUTTON_ID_ENTER2,
    BUTTON_ID_SEARCH,
    BUTTON_ID_WHEEL_DOWN,
    BUTTON_ID_WHEEL_LEFT,
    BUTTON_ID_WHEEL_RIGHT,
    BUTTON_ID_WHEEL_UP,
    BUTTON_ID_MAX
} Button_IdType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** Initializes every stored button input to the released level. */
void DrvButton_Init(void);

/**
 * \brief Reads the default APPS-button raw input.
 * \return BUTTON_RAW_STATE_PRESSED or BUTTON_RAW_STATE_RELEASED.
 */
uint8 DrvButton_Read(void);

/**
 * \brief Reads the raw input stored for a selected button.
 * \param[in] buttonId Button instance to read.
 * \return Stored raw level, or BUTTON_RAW_STATE_RELEASED for an invalid ID.
 */
uint8 DrvButton_ReadById(Button_IdType buttonId);

/**
 * \brief Updates the raw input stored for a selected button.
 * \param[in] buttonId Button instance to update.
 * \param[in] rawState Active-low pressed or released level.
 * \return STD_OK for valid input; otherwise STD_NOT_OK.
 */
Std_ReturnType DrvButton_SetInput(Button_IdType buttonId, uint8 rawState);

/**
 * \brief Sets the simulated pressed state of the default APPS button.
 * \param[in] isPressed STD_TRUE for pressed; otherwise released.
 */
void DrvButton_SetSimulatedInput(uint8 isPressed);

/**
 * \brief Sets the simulated pressed state of a selected button.
 * \param[in] buttonId Button instance to update.
 * \param[in] isPressed STD_TRUE for pressed; otherwise released.
 */
void DrvButton_SetSimulatedInputById(Button_IdType buttonId,
                                     uint8 isPressed);

#endif /* DRV_BUTTON_H */
