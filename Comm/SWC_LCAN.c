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
    \file   SWC_LCAN.c
    \brief  Implements the AUTOSAR RTE adapter for local-CAN communication.
            RTE data is read by SWC runnables and forwarded to the local-CAN
            input and output processing modules.
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
| 2026-08-26 000.01     DL      Creation
| 2026-10-26 000.02     DL      Standardized source documentation
| 2026-10-26 000.03     DL      Added knob-button report decoding and logging
| 2026-10-28 000.04     DL      Added validated HU message and timeout handling
| 2026-10-29 000.05     DL      Separated RTE adapter and CAN data processing
| 2026-10-29 000.06     DL      Added ComM mode query and request services
| 2026-10-29 000.07     DL      Used ComM service values for C/S operations
| 2026-10-30 000.08     DL      Aligned ComM values with generated RTE modes
| 2026-10-31 000.09     DL      Connected wheel and button CAN outputs
| 2026-11-01 000.10     DL      Added one-shot version-request responses
| 2026-11-06 000.11     DL      Added gated LCAN outputs and button diagnosis
| 2026-11-10 000.12     DL      Corrected version conversion and RTE outputs
| 2026-11-10 000.13     DL      Repeated version responses at 200 ms intervals
| 2026-11-10 000.14     DL      Moved version responses to lcan_version
| 2026-11-10 000.15     DL      Moved ComM services to lcan_comm
| 2026-11-10 000.16     DL      Named the default OTA update-status output
| 2026-11-11 000.17     DL      Invalidated all HU State data on timeout
| 2026-11-12 000.18     DL      Separated LCAN input RTE signal acquisition
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_LCAN.h"

#include "SWC_LCAN.h"
#include "SWC_PWR_MGR.h"
#include "prj_defs.h"

#include "button_app.h"
#include "lcan_comm.h"
#include "lcan_input.h"
#include "lcan_input_rte.h"
#include "lcan_output.h"
#include "lcan_version.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Direction-angle value indicating that touch-angle input is unsupported. */
#define LCAN_WHEEL_DIR_ANGLE_NOT_SUPPORTED ((uint16)0x01FFU)

/** Diagnostic touch-state value used when the feature is unsupported. */
#define LCAN_DIAG_TOUCH_STATE_INVALID ((uint8)0x0FU)

/** Diagnostic haptic-state value used when the feature is unsupported. */
#define LCAN_DIAG_HAPTIC_STATE_INVALID ((uint8)0x0FU)

/** Diagnostic rotate-change value used when the feature is unsupported. */
#define LCAN_DIAG_ROTATE_CHANGE_INVALID ((uint8)0x03U)

/** Diagnostic rotation-error fallback used when the feature is unsupported. */
#define LCAN_DIAG_ROTATION_ERROR_NOT_SUPPORTED ((uint8)0x00U)

/** Diagnostic value indicating normal CAN message communication. */
#define LCAN_DIAG_CAN_MESSAGE_NORMAL ((uint8)0x01U)

/** Diagnostic OTA update status used before a result is available. */
#define LCAN_DIAG_UPDATE_STATUS_DEFAULT ((uint8)0x00U)

/** Scroll encoder fallback used when the feature is unsupported. */
#define LCAN_SCROLL_ENCODER_NOT_SUPPORTED ((uint8)0xFFU)

/** Scroll key value used when the feature is unsupported. */
#define LCAN_SCROLL_KEY_INVALID ((uint8)0x0FU)

#if (PRJ_LCAN_LOG_OUTPUT_ENABLE == 1U)
/** LOG signal fallback used when the feature is unsupported. */
#define LCAN_LOG_SIGNAL_NOT_SUPPORTED ((uint8)0x00U)
#endif

#if (PRJ_LCAN_TOUCH_OUTPUT_ENABLE == 1U)
/** Touch force-state fallback used when the feature is unsupported. */
#define LCAN_TOUCH_FORCE_NOT_SUPPORTED ((uint8)0x00U)

/** Touch-count fallback used when the feature is unsupported. */
#define LCAN_TOUCH_COUNT_NOT_SUPPORTED ((uint8)0x00U)

/** Touch-position fallback used when the feature is unsupported. */
#define LCAN_TOUCH_POSITION_NOT_SUPPORTED ((uint8)0x00U)
#endif

/** Reset-key value used when the feature is unsupported. */
#define LCAN_RESET_KEY_INVALID ((uint8)0x0FU)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* Local-CAN data types are declared by the corresponding module headers. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Read-only LCAN input/output context aggregation for runtime observation. */
static SWC_LCAN_ContextType SWC_LCAN_Context =
{
    NULL_PTR,
    NULL_PTR
};

/** Indicates that the retained LCAN output values have been initialized. */
static boolean LCAN_OutputInitialized = FALSE;

/** Retains the LCAN transmission-permission result from the previous cycle. */
static boolean LCAN_TxPreviouslyAllowed = FALSE;

/** Requests one explicit button and wheel write after LCAN TX is enabled. */
static boolean LCAN_InitialOutputPending = FALSE;

/*******************************************************************************
    9. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static boolean LCAN_IsTxAllowed(void);
static void LCAN_EnsureOutputInitialized(void);
static void LCAN_WriteInitialOutputs(void);
static void LCAN_WriteDiagOutputs(void);
static void LCAN_WriteButtonOutputs(void);
#if (PRJ_LCAN_LOG_OUTPUT_ENABLE == 1U)
static void LCAN_WriteUnsupportedLogOutputs(void);
#endif
static void LCAN_WriteUnsupportedScrollOutputs(void);
#if (PRJ_LCAN_TOUCH_OUTPUT_ENABLE == 1U)
static void LCAN_WriteUnsupportedTouchOutputs(void);
#endif
static void LCAN_WriteUnsupportedResetOutputs(void);
static void LCAN_WriteWheelOutputs(void);

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_LCAN_START_SEC_CODE
#include "SWC_LCAN_MemMap.h"

/** \brief Returns the vehicle-specific engine or drive-ready condition. */
boolean SWC_LCAN_IsEngineStarted(void)
{
    return LcanInput_IsEngineStarted();
}

/** \brief Returns the combined read-only LCAN observation context. */
const SWC_LCAN_ContextType * SWC_LCAN_GetContext(void)
{
    LCAN_EnsureOutputInitialized();

    if (SWC_LCAN_Context.input == NULL_PTR)
    {
        SWC_LCAN_Context.input = LcanInput_GetContext();
    }
    else
    {
        /* The input context pointer is already connected. */
    }

    if (SWC_LCAN_Context.output == NULL_PTR)
    {
        SWC_LCAN_Context.output = LcanOutput_GetContext();
    }
    else
    {
        /* The output context pointer is already connected. */
    }

    return &SWC_LCAN_Context;
}

FUNC(void, SWC_LCAN_CODE) RE_LCAN_Input_10ms_Process(void)
{
    (void)SWC_LCAN_GetContext();
    LcanInput_MainFunction10ms();
}

FUNC(void, SWC_LCAN_CODE) RE_LCAN_Output_10ms_Process(void)
{
    const LcanInput_ContextType *inputContext = LcanInput_GetContext();
    boolean txAllowed;

    LCAN_EnsureOutputInitialized();
    LcanOutput_MainFunction10ms();
    txAllowed = LCAN_IsTxAllowed();

    if ((txAllowed == TRUE) && (LCAN_TxPreviouslyAllowed == FALSE))
    {
        LCAN_InitialOutputPending = TRUE;
    }
    else
    {
        /* No LCAN transmission-permission rising edge occurred. */
    }

    if (txAllowed == TRUE)
    {
        LCAN_WriteDiagOutputs();
        LcanVersion_MainFunction10ms(&inputContext->huSys);

#if (PRJ_LCAN_LOG_OUTPUT_ENABLE == 1U)
        LCAN_WriteUnsupportedLogOutputs();
#else
        /* Unsupported LOG RTE writes are excluded from this build. */
#endif

        LCAN_WriteUnsupportedScrollOutputs();

#if (PRJ_LCAN_TOUCH_OUTPUT_ENABLE == 1U)
        LCAN_WriteUnsupportedTouchOutputs();
#else
        /*
         * Touch RTE writes are disabled because these ports trigger
         * transmission on every Rte_Write call and Touch is unsupported.
         */
#endif

        LCAN_WriteUnsupportedResetOutputs();

        if (LCAN_InitialOutputPending == TRUE)
        {
            LCAN_WriteInitialOutputs();
            LCAN_InitialOutputPending = FALSE;
        }
        else
        {
            LCAN_WriteButtonOutputs();
            LCAN_WriteWheelOutputs();
        }
    }
    else
    {
        /* Retain the output snapshot until LCAN transmission is allowed. */
    }

    LCAN_TxPreviouslyAllowed = txAllowed;
}


/** \brief Reads and validates one received L_HU_CarInfo message. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_CarInfo(void)
{
    LcanInputRte_ReadHuCarInfo();
}

/** \brief Invalidates L_HU_CarInfo signals after a reception timeout. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_CarInfo_Timeout(void)
{
    LcanInputRte_NotifyHuCarInfoTimeout();
}

/** \brief Reads and validates one received L_HU_E_USM message. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_E_USM(void)
{
    LcanInputRte_ReadHuEUsm();
}

/** \brief Reads and validates one received L_HU_MoodInfo message. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_MoodInfo(void)
{
    LcanInputRte_ReadHuMoodInfo();
}

/** \brief Invalidates L_HU_MoodInfo signals after a reception timeout. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_MoodInfo_Timeout(void)
{
    LcanInputRte_NotifyHuMoodInfoTimeout();
}

/** \brief Reads and validates one received L_HU_SYS message. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_SYS(void)
{
    LcanInputRte_ReadHuSys();
}

/** \brief Reads and distributes one received L_HU_State message. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_State(void)
{
    LcanInputRte_ReadHuState();
}

/** \brief Invalidates L_HU_State data after an RTE reception timeout. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_State_Timeout(void)
{
    LcanInputRte_NotifyHuStateTimeout();
}

/** \brief Reads and validates one received L_HU_SysInfo message. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_SysInfo(void)
{
    LcanInputRte_ReadHuSysInfo();
}

/** \brief Invalidates L_HU_SysInfo signals after a reception timeout. */
FUNC(void, SWC_LCAN_CODE) RE_LCAN_L_HU_SysInfo_Timeout(void)
{
    LcanInputRte_NotifyHuSysInfoTimeout();
}

/** \brief Reads and forwards one wheel-direction report from the RTE. */
FUNC(void, SWC_LCAN_CODE) RE_SWC_Wheel_Direction_Event(void)
{
    uint8 rawData;
    Std_ReturnType rteResult;

    LCAN_EnsureOutputInitialized();
    rteResult = Rte_Read_R_u8_Wheel_Direction_KnobWheelDirection(&rawData);
    if (rteResult == E_OK)
    {
        LcanOutput_ReportWheel(rawData);
    }
    else
    {
        /* Keep the previous output data when the RTE read fails. */
    }
}

/** \brief Reads and forwards one packed knob-button report from the RTE. */
FUNC(void, SWC_LCAN_CODE) RE_SWC_Knob_Button_Event(void)
{
    uint16 rawData;
    Std_ReturnType rteResult;

    LCAN_EnsureOutputInitialized();
    rteResult = Rte_Read_R_u16_Knob_Button_KnobButtonDetected(&rawData);
    if (rteResult == E_OK)
    {
        LcanOutput_ReportButton(rawData);
    }
    else
    {
        /* Keep the previous output data when the RTE read fails. */
    }
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Evaluates the complete LCAN transmission-permission policy. */
static boolean LCAN_IsTxAllowed(void)
{
    LcanCom_ModeType currentMode = LCAN_COM_MODE_INVALID;
    boolean txAllowed = FALSE;

    if ((SWC_PWR_MGR_IsOperational() == TRUE) &&
        (SWC_PWR_MGR_IsBatteryOperationOn() == TRUE))
    {
        if ((LcanCom_GetCurrentMode(&currentMode) == E_OK) &&
            (currentMode == LCAN_COM_MODE_FULL_COM))
        {
            txAllowed = TRUE;
        }
        else
        {
            txAllowed = FALSE;
        }
    }
    else
    {
        txAllowed = FALSE;
    }

    return txAllowed;
}

/** \brief Initializes the LCAN output snapshot exactly once. */
static void LCAN_EnsureOutputInitialized(void)
{
    if (LCAN_OutputInitialized == FALSE)
    {
        LcanOutput_Init();
        LCAN_OutputInitialized = TRUE;
    }
    else
    {
        /* The retained output values are already initialized. */
    }
}

/** \brief Writes initial button and wheel values after LCAN TX is enabled. */
static void LCAN_WriteInitialOutputs(void)
{
    LCAN_WriteButtonOutputs();
    LCAN_WriteWheelOutputs();
}

/** \brief Writes the supported and fallback L_CCP_HU_PE_Diag signals. */
static void LCAN_WriteDiagOutputs(void)
{
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_BtnState(
        ButtonApp_GetDiagnosticStatus());
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_CanMsgErr(
        LCAN_DIAG_CAN_MESSAGE_NORMAL);
#if 0
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_ResetOpSta(
        PDC_Get_ResetOpSte());
#endif
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_UpdateStat(
        LCAN_DIAG_UPDATE_STATUS_DEFAULT);
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_TouchState(
        LCAN_DIAG_TOUCH_STATE_INVALID);
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_HptState(
        LCAN_DIAG_HAPTIC_STATE_INVALID);
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_RotateChangeSta(
        LCAN_DIAG_ROTATE_CHANGE_INVALID);
    (void)Rte_Write_LCAN_L_CCP_HU_PE_Diag_CCP_HU_RotationErrorSta(
        LCAN_DIAG_ROTATION_ERROR_NOT_SUPPORTED);
}

#if (PRJ_LCAN_LOG_OUTPUT_ENABLE == 1U)
/** \brief Writes fallback values for unsupported LOG signals. */
static void LCAN_WriteUnsupportedLogOutputs(void)
{
    (void)Rte_Write_LCAN_L_CCP_E_LOG_CCP_InfLoop1(
        LCAN_LOG_SIGNAL_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_LOG_CCP_InfLoop2(
        LCAN_LOG_SIGNAL_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_LOG_CCP_InfLoop3(
        LCAN_LOG_SIGNAL_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_LOG_CCP_InfLoop4(
        LCAN_LOG_SIGNAL_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_LOG_CCP_InfLoop5(
        LCAN_LOG_SIGNAL_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_LOG_CCP_InfLoop6(
        LCAN_LOG_SIGNAL_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_LOG_CCP_SlpWkup(
        LCAN_LOG_SIGNAL_NOT_SUPPORTED);
}
#endif

/** \brief Writes fallback values for unsupported scroll-control signals. */
static void LCAN_WriteUnsupportedScrollOutputs(void)
{
    (void)Rte_Write_LCAN_L_CCP_E_Scroll_CCP_TuneEncdr(
        LCAN_SCROLL_ENCODER_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Scroll_CCP_TuneKey(
        LCAN_SCROLL_KEY_INVALID);
    (void)Rte_Write_LCAN_L_CCP_E_Scroll_CCP_VolEncdr(
        LCAN_SCROLL_ENCODER_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Scroll_CCP_VolKey(
        LCAN_SCROLL_KEY_INVALID);
}

/** \brief Writes fallback values for unsupported touch-control signals. */
#if (PRJ_LCAN_TOUCH_OUTPUT_ENABLE == 1U)
static void LCAN_WriteUnsupportedTouchOutputs(void)
{
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_FrcState(
        LCAN_TOUCH_FORCE_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_TouchNum(
        LCAN_TOUCH_COUNT_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_TouchX1(
        LCAN_TOUCH_POSITION_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_TouchX2(
        LCAN_TOUCH_POSITION_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_TouchX3(
        LCAN_TOUCH_POSITION_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_TouchY1(
        LCAN_TOUCH_POSITION_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_TouchY2(
        LCAN_TOUCH_POSITION_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Touch_CCP_TouchY3(
        LCAN_TOUCH_POSITION_NOT_SUPPORTED);
}
#endif

/** \brief Writes the invalid key value for the unsupported reset input. */
static void LCAN_WriteUnsupportedResetOutputs(void)
{
    (void)Rte_Write_LCAN_L_CCP_HU_E_RST_CCP_RstKey(
        LCAN_RESET_KEY_INVALID);
}

/** \brief Writes knob-face button signals to the LCAN RTE ports. */
static void LCAN_WriteButtonOutputs(void)
{
    const LcanOutput_ContextType *context = LcanOutput_GetContext();

    (void)Rte_Write_LCAN_L_CCP_E_KEY_CCP_AppsKey(context->appsKey);
    (void)Rte_Write_LCAN_L_CCP_E_KEY_CCP_BckKey(context->backKey);
    (void)Rte_Write_LCAN_L_CCP_E_KEY_CCP_SearchKey(context->searchKey);

    (void)Rte_Write_LCAN_L_CCP_E_KEY_CCP_HomeKey(LCAN_OUTPUT_KEY_CLEAR);
    (void)Rte_Write_LCAN_L_CCP_E_KEY_CCP_MapKey(LCAN_OUTPUT_KEY_CLEAR);
    (void)Rte_Write_LCAN_L_CCP_E_KEY_CCP_MenuKey(LCAN_OUTPUT_KEY_CLEAR);
}

/** \brief Writes wheel encoder and wheel-button signals to LCAN RTE ports. */
static void LCAN_WriteWheelOutputs(void)
{
    const LcanOutput_ContextType *context = LcanOutput_GetContext();
    uint8 wheelEncoder = (uint8)0U;

    if (context->wheelEncoderValid == TRUE)
    {
        wheelEncoder = context->wheelEncoder;
    }
    else
    {
        /* Keep the initial encoder output at its default value. */
    }

    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_WhlEncdr(wheelEncoder);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_DirAng(
        LCAN_WHEEL_DIR_ANGLE_NOT_SUPPORTED);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_0_degree(
        context->direction0Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_45_degree(
        context->direction45Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_90_degree(
        context->direction90Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_135_degree(
        context->direction135Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_180_degree(
        context->direction180Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_225_degree(
        context->direction225Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_270_degree(
        context->direction270Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Dir_315_degree(
        context->direction315Degree);
    (void)Rte_Write_LCAN_L_CCP_E_Wheel_CCP_Enter(context->enterKey);
}

#define SWC_LCAN_STOP_SEC_CODE
#include "SWC_LCAN_MemMap.h"
