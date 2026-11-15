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
    \file   lcan_version.c
    \brief  Implements repeated LCAN version responses for OnChange signals.
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
| 2026-11-10 000.01     DL      Creation
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "Rte_SWC_LCAN.h"

#include "lcan_version.h"
#include "prj_common.h"
#include "prj_diag_data.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** HU request value that starts a corresponding version response. */
#define LCAN_VERSION_REQUEST_ACTIVE ((uint8)1U)

/** OnChange reset value written before each version response. */
#define LCAN_VERSION_CLEAR_VALUE ((uint8)0xFFU)

/** Number of successful transmissions generated for one version request. */
#define LCAN_VERSION_TX_REPEAT_COUNT ((uint8)3U)

/** Number of 10 ms function invocations corresponding to 200 ms. */
#define LCAN_VERSION_TX_INTERVAL_TICKS ((uint8)20U)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Function used to write one software, CAN DB, or hardware version pair. */
typedef Std_ReturnType (*LcanVersion_WriteFunctionType)(void);

/** Runtime state for one repeated version-response sequence. */
typedef struct
{
    boolean requestAccepted;
    boolean responseActive;
    uint8 transmissionCount;
    uint8 intervalTicks;
} LcanVersion_TxContextType;

/*******************************************************************************
    7. PRIVATE VARIABLES
*******************************************************************************/
/** Repeated response state for the software-version request. */
static LcanVersion_TxContextType LcanVersion_SoftwareTxContext =
{
    FALSE, FALSE, (uint8)0U, (uint8)0U
};

/** Repeated response state for the CAN DB-version request. */
static LcanVersion_TxContextType LcanVersion_CanTxContext =
{
    FALSE, FALSE, (uint8)0U, (uint8)0U
};

/** Repeated response state for the hardware-version request. */
static LcanVersion_TxContextType LcanVersion_HardwareTxContext =
{
    FALSE, FALSE, (uint8)0U, (uint8)0U
};

/*******************************************************************************
    8. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void LcanVersion_ProcessRequest(
    boolean requestValid,
    uint8 requestValue,
    LcanVersion_TxContextType * const context,
    LcanVersion_WriteFunctionType writeFunction);
static Std_ReturnType LcanVersion_WriteSoftware(void);
static Std_ReturnType LcanVersion_WriteCan(void);
static Std_ReturnType LcanVersion_WriteHardware(void);

/*******************************************************************************
    9. PUBLIC FUNCTIONS
*******************************************************************************/
#define SWC_LCAN_START_SEC_CODE
#include "SWC_LCAN_MemMap.h"

/** \brief Processes all HU version requests and response schedules. */
void LcanVersion_MainFunction10ms(
    const LcanInput_HuSysType * const huSys)
{
    if (huSys != NULL_PTR)
    {
        LcanVersion_ProcessRequest(
            huSys->softwareVersionRequestValid,
            huSys->softwareVersionRequest,
            &LcanVersion_SoftwareTxContext,
            LcanVersion_WriteSoftware);
        LcanVersion_ProcessRequest(
            huSys->canVersionRequestValid,
            huSys->canVersionRequest,
            &LcanVersion_CanTxContext,
            LcanVersion_WriteCan);
        LcanVersion_ProcessRequest(
            huSys->hardwareVersionRequestValid,
            huSys->hardwareVersionRequest,
            &LcanVersion_HardwareTxContext,
            LcanVersion_WriteHardware);
    }
    else
    {
        /* Reject a null HU request snapshot. */
    }
}

/*******************************************************************************
    10. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Runs one latched three-transmission version-response sequence. */
static void LcanVersion_ProcessRequest(
    boolean requestValid,
    uint8 requestValue,
    LcanVersion_TxContextType * const context,
    LcanVersion_WriteFunctionType writeFunction)
{
    boolean requestActive = FALSE;

    if ((requestValid == TRUE) &&
        (requestValue == LCAN_VERSION_REQUEST_ACTIVE))
    {
        requestActive = TRUE;
    }
    else
    {
        requestActive = FALSE;
    }

    if ((requestActive == TRUE) && (context->requestAccepted == FALSE))
    {
        context->requestAccepted = TRUE;
        context->responseActive = TRUE;
        context->transmissionCount = (uint8)0U;
        context->intervalTicks = (uint8)0U;
    }
    else if (requestActive == FALSE)
    {
        context->requestAccepted = FALSE;
    }
    else
    {
        /* Continue the response sequence for the accepted request. */
    }

    if (context->responseActive == TRUE)
    {
        if (context->intervalTicks > (uint8)0U)
        {
            context->intervalTicks--;
        }
        else
        {
            /* The first response is eligible immediately. */
        }

        if ((context->intervalTicks == (uint8)0U) &&
            (writeFunction() == E_OK))
        {
            context->transmissionCount++;

            if (context->transmissionCount >=
                LCAN_VERSION_TX_REPEAT_COUNT)
            {
                context->responseActive = FALSE;
            }
            else
            {
                context->intervalTicks =
                    LCAN_VERSION_TX_INTERVAL_TICKS;
            }
        }
        else
        {
            /* Retry a failed eligible write on the next 10 ms invocation. */
        }
    }
    else
    {
        /* No version response is currently scheduled. */
    }
}

/** \brief Writes the software-version response pair. */
static Std_ReturnType LcanVersion_WriteSoftware(void)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 versionValue = (uint16)0U;
    uint8 majorVersion;
    uint8 minorVersion;

    if (PrjCommon_ConvertVersion(
            ECU_SW_VER_DATA_INFO,
            &versionValue) == STD_OK)
    {
        majorVersion = (uint8)(versionValue >> 8U);
        minorVersion = (uint8)(versionValue & (uint16)0x00FFU);
        result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_SWVerMajor(
            LCAN_VERSION_CLEAR_VALUE);

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_SWVerMinor(
                LCAN_VERSION_CLEAR_VALUE);
        }
        else
        {
            /* Stop the sequence after a failed clear write. */
        }

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_SWVerMajor(
                majorVersion);
        }
        else
        {
            /* Stop the sequence after a failed clear write. */
        }

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_SWVerMinor(
                minorVersion);
        }
        else
        {
            /* Stop the sequence after an earlier failed write. */
        }
    }
    else
    {
        /* Reject malformed software-version configuration data. */
    }

    return result;
}

/** \brief Writes the CAN DB-version response pair. */
static Std_ReturnType LcanVersion_WriteCan(void)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 versionValue = (uint16)0U;
    uint8 majorVersion;
    uint8 minorVersion;

    if (PrjCommon_ConvertVersion(
            ECU_CANDB_VER_INFO,
            &versionValue) == STD_OK)
    {
        majorVersion = (uint8)(versionValue >> 8U);
        minorVersion = (uint8)(versionValue & (uint16)0x00FFU);
        result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_CANVerMajor(
            LCAN_VERSION_CLEAR_VALUE);

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_CANVerMinor(
                LCAN_VERSION_CLEAR_VALUE);
        }
        else
        {
            /* Stop the sequence after a failed clear write. */
        }

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_CANVerMajor(
                majorVersion);
        }
        else
        {
            /* Stop the sequence after a failed clear write. */
        }

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_CANVerMinor(
                minorVersion);
        }
        else
        {
            /* Stop the sequence after an earlier failed write. */
        }
    }
    else
    {
        /* Reject malformed CAN DB-version configuration data. */
    }

    return result;
}

/** \brief Writes the hardware-version response pair. */
static Std_ReturnType LcanVersion_WriteHardware(void)
{
    Std_ReturnType result = STD_NOT_OK;
    uint16 versionValue = (uint16)0U;
    uint8 majorVersion;
    uint8 minorVersion;

    if (PrjCommon_ConvertVersion(
            ECU_HW_VER_DATA_INFO,
            &versionValue) == STD_OK)
    {
        majorVersion = (uint8)(versionValue >> 8U);
        minorVersion = (uint8)(versionValue & (uint16)0x00FFU);
        result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_HWVerMajor(
            LCAN_VERSION_CLEAR_VALUE);

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_HWVerMinor(
                LCAN_VERSION_CLEAR_VALUE);
        }
        else
        {
            /* Stop the sequence after a failed clear write. */
        }

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_HWVerMajor(
                majorVersion);
        }
        else
        {
            /* Stop the sequence after a failed clear write. */
        }

        if (result == E_OK)
        {
            result = Rte_Write_LCAN_L_CCP_HU_E_SYS_CCP_HWVerMinor(
                minorVersion);
        }
        else
        {
            /* Stop the sequence after an earlier failed write. */
        }
    }
    else
    {
        /* Reject malformed hardware-version configuration data. */
    }

    return result;
}

#define SWC_LCAN_STOP_SEC_CODE
#include "SWC_LCAN_MemMap.h"
