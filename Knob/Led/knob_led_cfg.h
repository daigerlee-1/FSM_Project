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
    \file   knob_led_cfg.h
    \brief  Declares LED configuration limits and conversion services.
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
| 2026-10-15 000.01     DL      Creation
********************************************************************************/
#ifndef KNOB_LED_CFG_H
#define KNOB_LED_CFG_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Number of independently controlled E52248 LED channels. */
#define LED_CHANNEL_COUNT          (16U)

/** Selects all LED ports when passed to LedApp_RequestSetCurrent(). */
#define LED_PORT_MAX               ((uint8)LED_CHANNEL_COUNT)

/** Maximum valid 10-bit E52248 channel-current value. */
#define LED_CURRENT_VALUE_MAX      ((uint16)0x3FFU)

/** Minimum valid CLU automatic-brightness step. */
#define LED_BRIGHTNESS_STEP_MIN    ((uint8)2U)

/** First CLU automatic-brightness step that selects 100 percent duty. */
#define LED_BRIGHTNESS_STEP_FULL   ((uint8)101U)

/** Maximum valid CLU automatic-brightness step. */
#define LED_BRIGHTNESS_STEP_MAX    ((uint8)200U)

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
 * \brief Copies the default current configuration for all LED channels.
 *
 * \param[out] currentValues Destination array of LED_CHANNEL_COUNT elements.
 * \return STD_OK when copied; otherwise STD_NOT_OK.
 */
Std_ReturnType LedCfg_LoadDefaultCurrent(uint16 *currentValues);

/**
 * \brief Converts a CLU brightness step into an E52248 10-bit value.
 *
 * \param[in]  brightnessStep CLU brightness step from 2 through 200.
 * \param[out] brightnessValue Converted E52248 value.
 * \return STD_OK for a valid step and destination; otherwise STD_NOT_OK.
 */
Std_ReturnType LedCfg_GetBrightnessValue(uint8 brightnessStep,
                                         uint16 *brightnessValue);

/**
 * \brief Returns the startup brightness value.
 *
 * \return E52248 10-bit value corresponding to 50 percent duty.
 */
uint16 LedCfg_GetDefaultBrightnessValue(void);

#endif /* KNOB_LED_CFG_H */
