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
    \file   ccp_main.c
    \brief  Implements CCP services shared by host and AUTOSAR integrations.
            Windows and Linux builds additionally provide the program entry
            point and standalone 1 ms, 5 ms, and 10 ms scheduler.
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
| 2026-10-26 000.02     DL      Standardized C source documentation
| 2026-11-09 000.03     DL      Removed test-only master-key console output
| 2026-11-13 000.04     DL      Separated host platform system interfaces
| 2026-11-13 000.05     DL      Added IVD context observation
| 2026-11-13 000.06     DL      Separated host entry point from common services
| 2026-11-13 000.07     DL      Added AUTOSAR GPT time-base control
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_defs.h"

/* POSIX feature selection shall precede every system-header inclusion. */

#include "prj_typedefs.h"
#include "ccp_main.h"
#include "ccp_context.h"
#include "fsm_timer.h"

#include "Rte_SWC_CCP_Main.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** GPT counter value for a 1 ms period with a 1 MHz GPT input clock. */
#define CCP_GPT_1MS_TIMER_VALUE ((uint32)1000U)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void Main_InitContext(void);

static void GPT_Timer_Enable(uint32 timerValue);
static void GPT_Timer_Disable(void);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Aggregates read-only context pointers for diagnostics and test services. */
static Main_ContextType Main_Context;

/** Indicates that the AUTOSAR GPT time base accepts notifications. */
static volatile boolean GPT_TimerEnabled = FALSE;

/*******************************************************************************
    9. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/* No private constants or configuration tables are defined by this module. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes common services and starts the AUTOSAR 1 ms GPT. */
void CCP_Init(void)
{
    FSM_Timer_Init();
    Main_InitContext();
    GPT_Timer_Enable(CCP_GPT_1MS_TIMER_VALUE);

#if (I2C_LED_TEST_ENABLE == STD_OFF)
    LedApp_Init();
#endif
}

/** \brief Handles an enabled AUTOSAR GPT notification. */
void GPT_Timer_Notification(void)
{
    if (GPT_TimerEnabled == TRUE)
    {
        FSM_Timer_Notification();
    }
    else
    {
        /* Ignore notifications received before GPT initialization. */
    }
}


/** \brief Returns the aggregated read-only application context. */
const Main_ContextType * Main_GetContext(void)
{
    return &Main_Context;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Connects each feature context to the top-level diagnostic context. */
static void Main_InitContext(void)
{
    uint8 index;

    Main_Context.power = SWC_PWR_MGR_GetContext();
    Main_Context.led = LedApp_GetContext();
    Main_Context.button = ButtonApp_GetContext();
    for (index = (uint8)0U; index < (uint8)BUTTON_ID_MAX; index++)
    {
        Main_Context.buttons[index] =
            ButtonApp_GetContextById((Button_IdType)index);
    }
    Main_Context.canInput = LcanInput_GetContext();
    Main_Context.canOutput = LcanOutput_GetContext();
    Main_Context.nvm = NvmApp_GetContext();
    Main_Context.wheel = WheelApp_GetContext();
    Main_Context.ivd = CcpIvdCheck_GetContext();
}

/** \brief Starts or restarts the AUTOSAR GPT 1 ms time base. */
static void GPT_Timer_Enable(uint32 timerValue)
{
    if (GPT_TimerEnabled == TRUE)
    {
        GPT_Timer_Disable();
    }
    else
    {
        /* The GPT is already stopped. */
    }

    (void)Rte_Call_R_Gpt_FsmTimer1ms_EnableNotification();
    GPT_TimerEnabled = TRUE;
    (void)Rte_Call_R_Gpt_FsmTimer1ms_StartTimer(
        (IoHwAb_GptValueType)timerValue);
}

/** \brief Stops the AUTOSAR GPT and rejects subsequent notifications. */
static void GPT_Timer_Disable(void)
{
    GPT_TimerEnabled = FALSE;
    (void)Rte_Call_R_Gpt_FsmTimer1ms_DisableNotification();
    (void)Rte_Call_R_Gpt_FsmTimer1ms_StopTimer();
}

