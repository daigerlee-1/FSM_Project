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
    \file   SWC_KnobLed.c
    \brief  Implements AUTOSAR runnable entities for knob LED control.
            This module forwards I2C notifications to the E52248 communication
            service and invokes the cyclic LED application processing.
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
| 2026-10-16 000.02     DL      Standardized C source documentation
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_KnobLed.h"

#include "prj_typedefs.h"
#include "knob_led.h"
#include "e52248_comm.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/* No private definitions or macros are declared by this module. */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

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
#define SWC_KnobLed_START_SEC_CODE
#include "SWC_KnobLed_MemMap.h"

/** \brief Forwards an SCB3 I2C error notification to the E52248 service. */
FUNC(void, SWC_KnobLed_CODE) RE_KnobLed_SCB3_Error_Notification(void)
{
    E52248_Comm_ErrorNotification();
}

/** \brief Forwards an SCB3 I2C receive notification to the E52248 service. */
FUNC(void, SWC_KnobLed_CODE) RE_KnobLed_SCB3_Rx_Notification(void)
{
    E52248_Comm_RxNotification();
}

/** \brief Forwards an SCB3 I2C transmit notification to the E52248 service. */
FUNC(void, SWC_KnobLed_CODE) RE_KnobLed_SCB3_Tx_Notification(void)
{
    E52248_Comm_TxNotification();
}

/** \brief Forwards an SCB7 I2C error notification to the E52248 service. */
FUNC(void, SWC_KnobLed_CODE) RE_KnobLed_SCB7_Error_Notification(void)
{
    E52248_Comm_ErrorNotification();
}

/** \brief Forwards an SCB7 I2C receive notification to the E52248 service. */
FUNC(void, SWC_KnobLed_CODE) RE_KnobLed_SCB7_Rx_Notification(void)
{
    E52248_Comm_RxNotification();
}

/** \brief Forwards an SCB7 I2C transmit notification to the E52248 service. */
FUNC(void, SWC_KnobLed_CODE) RE_KnobLed_SCB7_Tx_Notification(void)
{
    E52248_Comm_TxNotification();
}

#define TEST_LED_CODE 1
#ifdef TEST_LED_CODE 
uint8 test_led_code_val = 0U;
uint8 test_led_bright = 0U;
uint8 test_led_curr_port = 0U;
uint16 test_led_curr_value = 0U;
#endif
/** \brief Executes the 10 ms knob LED application FSM processing. */
FUNC(void, SWC_KnobLed_CODE) RE_SWC_KnobLed_10ms_Task(void)
{
    LedApp_MainFunction10ms();
#ifdef TEST_LED_CODE 
    if (test_led_code_val == 1U)
    {
        test_led_code_val = 0U;
        LedApp_RequestSetBrightness(test_led_bright);
    }
    else if (test_led_code_val == 2U)
    {
        test_led_code_val = 0U;
        LedApp_RequestSetCurrent(test_led_curr_port, test_led_curr_value);
    }
    else if (test_led_code_val == 3U)
    {
        test_led_code_val = 0U;
        LedApp_RequestReadVled();
    }
    else if (test_led_code_val == 4U)
    {
        test_led_code_val = 0U;
        LedApp_RequestReadVdif();
    }
    else if (test_led_code_val == 5U)
    {
        test_led_code_val = 0U;
        LedApp_RequestReadIled();
    }
    else if (test_led_code_val == 6U)
    {
        test_led_code_val = 0U;
        LedApp_RequestReadTemperature();
    }
	else if (test_led_code_val == 7U)
    {
        test_led_code_val = 0U;
        LedApp_RequestOtpValidation();
    }
	else if (test_led_code_val == 8U)
    {
        test_led_code_val = 0U;
#if (PRJ_LED_OTP_TEST_WRITE_ENABLE == 1U)
        LedApp_RequestOtpWrite();
#endif		
    }
	else if (test_led_code_val == 9U)
    {
        test_led_code_val = 0U;
#if (PRJ_LED_OPEN_SHORT_TEST_ENABLE == 1U)
        LedApp_RequestReadOpenShort();
#endif		
    }
#endif		
}

/** \brief Executes the 5 ms E52248 communication FSM processing. */
FUNC(void, SWC_KnobLed_CODE) RE_SWC_KnobLed_5ms_Task(void)
{
    LedApp_MainFunction5ms();
}

#define SWC_KnobLed_STOP_SEC_CODE
#include "SWC_KnobLed_MemMap.h"

