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
    \file   SWC_CCP_Main.c
    \brief  Implements the AUTOSAR RTE adapter for CCP main scheduling.
            The module initializes common CCP services once, provides cyclic
            main runnables, and forwards the 1 ms GPT notification to the
            shared FSM timer service.
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
| 2026-11-13 000.02     DL      Standardized CCP main SWC documentation
| 2026-11-13 000.03     DL      Scheduled nonblocking IVD calculation
********************************************************************************/


/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_CCP_Main.h"

#include "SWC_CCP_Main.h"
#include "prj_typedefs.h"

#include "ccp_main.h"
#include "ccp_ivd_check.h"

#if (I2C_LED_TEST_ENABLE == STD_ON)
#include "knob_led.h"
#endif

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
/* No public variables are defined by this module. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Indicates that the common CCP services have been initialized. */
static uint8 isMainInit = FALSE;

#if (I2C_LED_TEST_ENABLE == STD_ON)
/** Debug-controlled request state for restarting the LED application. */
static uint8 isLedInit = FALSE;

/** Debug-controlled trigger for processing one LED initialization request. */
static uint8 isLedInitStart = FALSE;
#endif

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private function prototypes are required. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_CCP_Main_START_SEC_CODE
#include "SWC_CCP_Main_MemMap.h"

/**
 * \brief Initializes CCP once and processes optional LED test initialization.
 */
FUNC(void, SWC_CCP_Main_CODE) RE_SWC_CCP_Main_10ms_Task(void)
{
    if (isMainInit == FALSE)
    {
        isMainInit = TRUE;
        CCP_Init();
        CcpIvdCheck_Init();
    }
    else
    {
        /* Common CCP initialization is performed only once. */
    }

#if (I2C_LED_TEST_ENABLE == STD_ON)
    /* Process a debugger-triggered LED initialization request once. */
    if (isLedInitStart == TRUE)
    {
        isLedInitStart = FALSE;

        if (isLedInit == TRUE)
        {
            isLedInit = FALSE;
            LedApp_Init();
        }
        else
        {
            /* No LED initialization was requested. */
        }
    }
    else
    {
        /* No debugger-triggered LED request is pending. */
    }
#else
    /* LED initialization test support is excluded from this build. */
#endif
}

/** \brief Advances the nonblocking IVD calculation every 5 ms. */
FUNC(void, SWC_CCP_Main_CODE) RE_SWC_CCP_Main_5ms_Task(void)
{
    CcpIvdCheck_MainFunction5ms();
}

/** \brief Forwards the 1 ms GPT callback to the shared FSM timer service. */
FUNC(void, SWC_CCP_Main_CODE) RE_Gpt_FsmTimer1ms_Cbk(void)
{
    GPT_Timer_Notification();
}

#define SWC_CCP_Main_STOP_SEC_CODE
#include "SWC_CCP_Main_MemMap.h"

