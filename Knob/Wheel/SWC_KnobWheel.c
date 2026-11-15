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
    \file   SWC_KnobWheel.c
    \brief  Adapts AUTOSAR RTE services to the wheel application.
            This module configures ICU notifications, reads photo-sensor
            levels, forwards snapshots, and publishes direction reports.
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
| 2026-10-16 000.02     DL      Separated AUTOSAR and application processing
| 2026-10-26 000.03     DL      Limited ICU callbacks to sensor event capture
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_KnobWheel.h"

#include "prj_typedefs.h"

#include "SWC_KnobWheel.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/* No private definitions or macros are declared by this module. */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static Std_ReturnType SwcKnobWheel_EnableIcuNotifications(void);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Indicates that both the wheel application and ICU services are initialized. */
static boolean SwcKnobWheel_Initialized = FALSE;

/*******************************************************************************
    9. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/* No private constants or configuration tables are defined by this module. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_KnobWheel_START_SEC_CODE
#include "SWC_KnobWheel_MemMap.h"

/** \brief Runs the wheel application and publishes one pending report. */
FUNC(void, SWC_KnobWheel_CODE) RE_ICU_Photo_Int_10ms_Task(void)
{
    uint8 reportData;

    if (SwcKnobWheel_Initialized == FALSE)
    {
        if ((WheelApp_Init() == STD_OK) &&
            (SwcKnobWheel_EnableIcuNotifications() == STD_OK))
        {
            SwcKnobWheel_Initialized = TRUE;
        }
        else
        {
            SwcKnobWheel_Initialized = FALSE;
        }
    }
    else
    {
        /* Initialization is performed only once. */
    }

    if (SwcKnobWheel_Initialized == TRUE)
    {
        WheelApp_MainFunction10ms();
        if (WheelApp_GetReport(&reportData) == STD_OK)
        {
            if (Rte_Write_P_u8_Wheel_Direction_KnobWheelDirection(
                    reportData) == E_OK)
            {
                WheelApp_ConfirmReport();
            }
            else
            {
                /* Keep the report pending and retry on the next activation. */
            }
        }
        else
        {
            /* No direction report is pending. */
        }
    }
    else
    {
        /* Cyclic processing remains disabled after initialization failure. */
    }
}

/** \brief Reads both sensors and queues an INT1-edge snapshot from the ISR. */
FUNC(void, SWC_KnobWheel_CODE) RE_PHOTO_INT1_Cbk(void)
{
    Std_ReturnType photo1Result;
    Std_ReturnType photo2Result;
    IoHwAb_LevelType photo1Level;
    IoHwAb_LevelType photo2Level;

    photo1Result =
        Rte_Call_R_DigDir_WHEEL_PHOTO_INT1_Input_ReadDirect(&photo1Level);
    photo2Result =
        Rte_Call_R_DigDir_WHEEL_PHOTO_INT2_Input_ReadDirect(&photo2Level);

    if ((photo1Result == E_OK) && (photo2Result == E_OK))
    {
        (void)WheelApp_NotifyPhoto1((uint8)photo1Level,
                                    (uint8)photo2Level);
    }
    else
    {
        /* Discard the edge when either sensor read fails. */
    }
}

/** \brief Reads both sensors and queues an INT2-edge snapshot from the ISR. */
FUNC(void, SWC_KnobWheel_CODE) RE_PHOTO_INT2_Cbk(void)
{
    Std_ReturnType photo1Result;
    Std_ReturnType photo2Result;
    IoHwAb_LevelType photo1Level;
    IoHwAb_LevelType photo2Level;

    photo1Result =
        Rte_Call_R_DigDir_WHEEL_PHOTO_INT1_Input_ReadDirect(&photo1Level);
    photo2Result =
        Rte_Call_R_DigDir_WHEEL_PHOTO_INT2_Input_ReadDirect(&photo2Level);

    if ((photo1Result == E_OK) && (photo2Result == E_OK))
    {
        (void)WheelApp_NotifyPhoto2((uint8)photo1Level,
                                    (uint8)photo2Level);
    }
    else
    {
        /* Discard the edge when either sensor read fails. */
    }
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Configures and enables both-edge ICU notifications. */
static Std_ReturnType SwcKnobWheel_EnableIcuNotifications(void)
{
    Std_ReturnType result = STD_OK;

    if (Rte_Call_R_IcuLogical_Photo_Int1_SetActivationCondition(
            IOHWAB_BOTH_EDGES) != E_OK)
    {
        result = STD_NOT_OK;
    }
    if (Rte_Call_R_IcuLogical_Photo_Int2_SetActivationCondition(
            IOHWAB_BOTH_EDGES) != E_OK)
    {
        result = STD_NOT_OK;
    }
    if (Rte_Call_R_IcuLogical_Photo_Int1_EnableEdgeDetection() != E_OK)
    {
        result = STD_NOT_OK;
    }
    if (Rte_Call_R_IcuLogical_Photo_Int2_EnableEdgeDetection() != E_OK)
    {
        result = STD_NOT_OK;
    }
    if (Rte_Call_R_IcuLogical_Photo_Int1_EnableNotification() != E_OK)
    {
        result = STD_NOT_OK;
    }
    if (Rte_Call_R_IcuLogical_Photo_Int2_EnableNotification() != E_OK)
    {
        result = STD_NOT_OK;
    }

    return result;
}

#define SWC_KnobWheel_STOP_SEC_CODE
#include "SWC_KnobWheel_MemMap.h"
