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
#if ((PRJ_MOBILGENE_ENABLE == 0U) && (PLATFORM_LINUX == 1U))
    #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 199309L
    #endif
#endif

#include "prj_typedefs.h"
#include "ccp_main.h"
#include "ccp_context.h"
#include "fsm_timer.h"

#if (PRJ_MOBILGENE_ENABLE == 1U)
#include "Rte_SWC_CCP_Main.h"
#else
#include "task.h"

#if (TEST_CASE_ENABLE == 1U)
#include "testcase.h"
#endif

#include "drv_console.h"

#if (PLATFORM_WINDOWS == 1U)
#include <windows.h>
#elif (PLATFORM_LINUX == 1U)
#include <time.h>
#endif
#endif /* PRJ_MOBILGENE_ENABLE == 1U */

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
#if (PRJ_MOBILGENE_ENABLE == 1U)
/** GPT counter value for a 1 ms period with a 1 MHz GPT input clock. */
#define CCP_GPT_1MS_TIMER_VALUE ((uint32)1000U)
#else
/** Base period used to advance the software scheduler. */
#define TASK_1MS_PERIOD_MS   ((uint32)1U)

/** Number of base ticks between 5 ms task activations. */
#define TASK_5MS_PERIOD_MS   ((uint32)5U)

/** Number of base ticks between 10 ms task activations. */
#define TASK_10MS_PERIOD_MS  ((uint32)10U)
#endif /* PRJ_MOBILGENE_ENABLE == 1U */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void Main_InitContext(void);

#if (PRJ_MOBILGENE_ENABLE == 1U)
static void GPT_Timer_Enable(uint32 timerValue);
static void GPT_Timer_Disable(void);
#else
static void Main_SleepMs(uint32 sleepMs);
static uint8 Main_IsRunning(void);
#endif /* PRJ_MOBILGENE_ENABLE == 1U */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Aggregates read-only context pointers for diagnostics and test services. */
static Main_ContextType Main_Context;

#if (PRJ_MOBILGENE_ENABLE == 1U)
/** Indicates that the AUTOSAR GPT time base accepts notifications. */
static volatile boolean GPT_TimerEnabled = FALSE;
#endif

/*******************************************************************************
    9. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/* No private constants or configuration tables are defined by this module. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#if (PRJ_MOBILGENE_ENABLE == 1U)
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

#else

/** \brief Initializes services used by the standalone integration. */
void CCP_Init(void)
{
    Main_InitContext();
}

/** \brief Provides a compatible software-timer notification entry point. */
void GPT_Timer_Notification(void)
{
    FSM_Timer_Notification();
}

/**
 * \brief Initializes CCP services and runs the standalone cyclic scheduler.
 * \return Zero after the interactive test loop requests program termination.
 */
int main(void)
{
    uint8 tick5msCount = (uint8)0U;
    uint8 tick10msCount = (uint8)0U;

    DrvConsole_Init();
    Task_Init();
    CCP_Init();

    while (Main_IsRunning() == STD_TRUE)
    {
        Task_1ms();
        tick5msCount++;
        tick10msCount++;

        if (tick5msCount >= (uint8)5U)
        {
            tick5msCount = (uint8)0U;
            Task_5ms();
        }
        else
        {
            /* Wait until five 1 ms base ticks have elapsed. */
        }

        if (tick10msCount >= (uint8)10U)
        {
            tick10msCount = (uint8)0U;
            Task_10ms();
        }
        else
        {
            /* Wait until ten 1 ms base ticks have elapsed. */
        }

        Main_SleepMs(TASK_1MS_PERIOD_MS);
    }

    DrvConsole_DeInit();
    DrvConsole_WriteString("\r\nbye\r\n");

    return 0;
}
#endif /* PRJ_MOBILGENE_ENABLE == 1U */

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

#if (PRJ_MOBILGENE_ENABLE == 1U)
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

#else

/** \brief Reports whether the standalone scheduler shall continue running. */
static uint8 Main_IsRunning(void)
{
    uint8 isRunning = STD_TRUE;

#if (TEST_CASE_ENABLE == 1U)
    if (TestCase_IsExitRequested() == STD_TRUE)
    {
        isRunning = STD_FALSE;
    }
    else
    {
        isRunning = STD_TRUE;
    }
#endif

    return isRunning;
}

/** \brief Suspends standalone execution for the requested milliseconds. */
static void Main_SleepMs(uint32 sleepMs)
{
#if (PLATFORM_WINDOWS == 1U)
    Sleep((DWORD)sleepMs);
#elif (PLATFORM_LINUX == 1U)
    struct timespec request;
    struct timespec remain;

    request.tv_sec = (time_t)(sleepMs / (uint32)1000U);
    request.tv_nsec =
        (long)((sleepMs % (uint32)1000U) * (uint32)1000000U);
    remain.tv_sec = (time_t)0;
    remain.tv_nsec = (long)0;

    (void)nanosleep(&request, &remain);
#else
    (void)sleepMs;
#endif
}

#endif /* PRJ_MOBILGENE_ENABLE == 1U */
