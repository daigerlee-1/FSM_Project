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
    \file   knob_led_cfg.c
    \brief  Implements LED defaults and brightness-table conversion.
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
| 2026-11-11 000.02     DL      Updated channel current defaults and comments
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "knob_led_cfg.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** E52248 10-bit brightness value corresponding to 100 percent. */
#define LEDCFG_BRIGHTNESS_100_PERCENT ((uint16)0x3FFU)

/** E52248 10-bit brightness value corresponding to 50 percent. */
#define LEDCFG_BRIGHTNESS_50_PERCENT  ((uint16)0x200U)

/** Number of entries covering CLU brightness steps 2 through 100. */
#define LEDCFG_BRIGHTNESS_TABLE_SIZE                                  \
    ((uint8)(LED_BRIGHTNESS_STEP_FULL - LED_BRIGHTNESS_STEP_MIN))

/** Returns the number of elements in a statically allocated array. */
#define LEDCFG_ARRAY_SIZE(array)                                      \
    ((uint32)(sizeof(array) / sizeof((array)[0])))

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private functions are declared by this module. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/* No private variables are declared by this module. */

/*******************************************************************************
    9. PRIVATE CONSTANTS
*******************************************************************************/
/**
 * Default E52248 current configuration.
 *
 * Array index corresponds to the E52248 LED channel number.
 */
static const uint16 LedCfg_DefaultCurrentValues[LED_CHANNEL_COUNT] =
{
#if 0
    (uint16)0U,   (uint16)0U,   (uint16)300U, (uint16)300U,
    (uint16)300U, (uint16)300U, (uint16)300U, (uint16)300U,
    (uint16)300U, (uint16)300U, (uint16)300U, (uint16)200U,
    (uint16)200U, (uint16)200U, (uint16)0U,   (uint16)0U
#else
    (uint16)0U,  (uint16)0U,  (uint16)30U, (uint16)30U, /* CH00-CH03 */
    (uint16)30U, (uint16)30U, (uint16)30U, (uint16)30U, /* CH04-CH07 */
    (uint16)30U, (uint16)30U, (uint16)0U,  (uint16)0U,  /* CH08-CH11 */
    (uint16)0U,  (uint16)0U,  (uint16)0U,  (uint16)0U   /* CH12-CH15 */
#endif
};

/** E52248 values converted from the duty table for steps 2 through 100. */
static uint16 const LedCfg_BrightnessTable[] =
{
    (uint16)10U,  (uint16)17U,  (uint16)25U,  (uint16)32U,
    (uint16)39U,  (uint16)47U,  (uint16)54U,  (uint16)61U,
    (uint16)69U,  (uint16)76U,  (uint16)83U,  (uint16)90U,
    (uint16)98U,  (uint16)105U, (uint16)112U, (uint16)119U,
    (uint16)127U, (uint16)134U, (uint16)141U, (uint16)149U,
    (uint16)156U, (uint16)163U, (uint16)171U, (uint16)173U,
    (uint16)176U, (uint16)178U, (uint16)181U, (uint16)183U,
    (uint16)186U, (uint16)189U, (uint16)191U, (uint16)194U,
    (uint16)196U, (uint16)199U, (uint16)202U, (uint16)204U,
    (uint16)207U, (uint16)209U, (uint16)212U, (uint16)214U,
    (uint16)217U, (uint16)220U, (uint16)222U, (uint16)225U,
    (uint16)227U, (uint16)230U, (uint16)233U, (uint16)235U,
    (uint16)238U, (uint16)240U, (uint16)243U, (uint16)245U,
    (uint16)248U, (uint16)251U, (uint16)253U, (uint16)256U,
    (uint16)258U, (uint16)261U, (uint16)264U, (uint16)266U,
    (uint16)269U, (uint16)271U, (uint16)274U, (uint16)276U,
    (uint16)279U, (uint16)282U, (uint16)284U, (uint16)287U,
    (uint16)289U, (uint16)292U, (uint16)295U, (uint16)297U,
    (uint16)300U, (uint16)302U, (uint16)305U, (uint16)307U,
    (uint16)310U, (uint16)313U, (uint16)315U, (uint16)318U,
    (uint16)320U, (uint16)323U, (uint16)326U, (uint16)328U,
    (uint16)331U, (uint16)333U, (uint16)336U, (uint16)338U,
    (uint16)341U, (uint16)376U, (uint16)416U, (uint16)460U,
    (uint16)508U, (uint16)562U, (uint16)621U, (uint16)686U,
    (uint16)758U, (uint16)838U, (uint16)926U
};

/** Compile-time check that every table-based brightness step is defined. */
typedef char LedCfg_BrightnessTableSizeCheckType[
    (LEDCFG_ARRAY_SIZE(LedCfg_BrightnessTable) ==
     (uint32)LEDCFG_BRIGHTNESS_TABLE_SIZE) ? 1 : -1];

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Copies the default current values into the caller's array. */
Std_ReturnType LedCfg_LoadDefaultCurrent(uint16 *currentValues)
{
    Std_ReturnType result = STD_NOT_OK;
    uint8 index;

    if (currentValues != NULL_PTR)
    {
        for (index = (uint8)0U;
             index < (uint8)LED_CHANNEL_COUNT;
             index++)
        {
            currentValues[index] = LedCfg_DefaultCurrentValues[index];
        }

        result = STD_OK;
    }
    else
    {
        /* A complete destination array is required. */
    }

    return result;
}

/** \brief Converts a validated CLU brightness step to an E52248 value. */
Std_ReturnType LedCfg_GetBrightnessValue(uint8 brightnessStep,
                                         uint16 *brightnessValue)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((brightnessValue != NULL_PTR) &&
        (brightnessStep >= LED_BRIGHTNESS_STEP_MIN) &&
        (brightnessStep <= LED_BRIGHTNESS_STEP_MAX))
    {
        if (brightnessStep >= LED_BRIGHTNESS_STEP_FULL)
        {
            *brightnessValue = LEDCFG_BRIGHTNESS_100_PERCENT;
        }
        else
        {
            *brightnessValue = LedCfg_BrightnessTable[
                (uint8)(brightnessStep - LED_BRIGHTNESS_STEP_MIN)];
        }

        result = STD_OK;
    }
    else
    {
        /* Invalid steps and destination pointers are rejected. */
    }

    return result;
}

/** \brief Returns the startup brightness value. */
uint16 LedCfg_GetDefaultBrightnessValue(void)
{
    return LEDCFG_BRIGHTNESS_50_PERCENT;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/* No private function implementations are defined by this module. */
