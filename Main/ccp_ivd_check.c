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
    \file   ccp_ivd_check.c
    \brief  Implements nonblocking CCP Integrity Validation Data processing.
*******************************************************************************
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
| 2026-11-13 000.01     DL      Creation
| 2026-11-13 000.02     DL      Added nonblocking SecureFlash calculation
*******************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "ccp_ivd_check.h"
#include "SWC_PWR_MGR.h"

#if (PRJ_IVD_CALCULATION_ENABLE == 1U)
#include "HKMC_SecureFlash2.h"
#endif

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Maximum value retained by the diagnostic check counter. */
#define CCP_IVD_CHECK_COUNT_MAX ((uint32)0xFFFFFFFFU)

#define CCP_IVD_SECTION1_LENGTH \
    ((uint32)((PRJ_IVD_SECTION1_END_ADDRESS - \
               PRJ_IVD_SECTION1_START_ADDRESS) + 1U))

#define CCP_IVD_SECTION2_LENGTH \
    ((uint32)((PRJ_IVD_SECTION2_END_ADDRESS - \
               PRJ_IVD_SECTION2_START_ADDRESS) + 1U))

#if (PRJ_IVD_CALCULATION_ENABLE == 1U)
        #define CCP_IVD_ADDRESS_TO_POINTER(address) \
            ((const uint8 *)(uintptr_t)(address))
#endif

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Internal stages of the asynchronous hash calculation. */
typedef enum
{
    CCP_IVD_CALC_STATE_IDLE = 0U,
    CCP_IVD_CALC_STATE_START,
    CCP_IVD_CALC_STATE_SECTION1,
    CCP_IVD_CALC_STATE_SECTION2,
    CCP_IVD_CALC_STATE_FINISH,
    CCP_IVD_CALC_STATE_COMPLETE,
    CCP_IVD_CALC_STATE_ERROR,
    CCP_IVD_CALC_STATE_MAX
} CcpIvdCheck_CalculationStateType;

/** Internal cursor used to process configured flash ranges incrementally. */
typedef struct
{
    CcpIvdCheck_CalculationStateType state;
    uint32 currentAddress;
    uint32 remainingLength;
} CcpIvdCheck_CalculationContextType;

/*******************************************************************************
    7. PRIVATE VARIABLES
*******************************************************************************/
/** Module-owned result and observable calculation status. */
static CcpIvdCheck_ContextType CcpIvdCheck_Context;

/** Module-owned cursor for the active SecureFlash calculation. */
static CcpIvdCheck_CalculationContextType CcpIvdCheck_CalculationContext;

/** Calculated SHA-256 digest. */
static uint8 CcpIvdCheck_CalculatedData[PRJ_IVD_DIGEST_LENGTH];

/*******************************************************************************
    8. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static void CcpIvdCheck_ClearCalculatedData(void);
static void CcpIvdCheck_IncrementCheckCount(void);
static void CcpIvdCheck_ResetCalculation(void);

#if (PRJ_IVD_CALCULATION_ENABLE == 1U)
static void CcpIvdCheck_ProcessCalculation(void);
static void CcpIvdCheck_ProcessFlashSection(
    CcpIvdCheck_CalculationStateType nextState);
static void CcpIvdCheck_SelectSection(
    uint32 startAddress,
    uint32 sectionLength,
    CcpIvdCheck_CalculationStateType state);
static void CcpIvdCheck_SetCalculationError(void);
#endif

/*******************************************************************************
    9. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Initializes all IVD check data to deterministic values. */
void CcpIvdCheck_Init(void)
{
    CcpIvdCheck_Context.result = CCP_IVD_CHECK_RESULT_NOT_EXECUTED;
    CcpIvdCheck_Context.initialized = TRUE;
    CcpIvdCheck_Context.calculationActive = FALSE;
    CcpIvdCheck_Context.calculationComplete = FALSE;
    CcpIvdCheck_Context.calculationError = FALSE;
    CcpIvdCheck_Context.calculationElapsedTicks = (uint32)0U;
    CcpIvdCheck_Context.checkCount = (uint32)0U;

    CcpIvdCheck_ClearCalculatedData();
    CcpIvdCheck_ResetCalculation();
}

/** \brief Advances the nonblocking SecureFlash calculation by one step. */
void CcpIvdCheck_MainFunction5ms(void)
{
    if (CcpIvdCheck_Context.initialized == FALSE)
    {
        CcpIvdCheck_Context.result = CCP_IVD_CHECK_RESULT_ERROR;
    }
    else
    {
#if (PRJ_IVD_CALCULATION_ENABLE == 1U)
        if (CcpIvdCheck_Context.calculationActive == TRUE)
        {
            if (CcpIvdCheck_Context.calculationElapsedTicks <
                (uint32)PRJ_IVD_CALCULATION_TIMEOUT_TICKS)
            {
                CcpIvdCheck_Context.calculationElapsedTicks++;
            }
            else
            {
                CcpIvdCheck_SetCalculationError();
            }
        }
        else
        {
            /* The timeout is counted only while calculation is active. */
        }

        if (SWC_PWR_MGR_IsBatteryOperationOn() == FALSE)
        {
            if (CcpIvdCheck_Context.calculationActive == TRUE)
            {
                CcpIvdCheck_ResetCalculation();
            }
            else
            {
                /* No active calculation needs to be aborted. */
            }
        }
        else
        {
            CcpIvdCheck_ProcessCalculation();
        }
#else
        /* SecureFlash calculation is excluded from this build. */
#endif
    }
}

/** \brief Compares complete calculated and expected IVD byte sequences. */
Std_ReturnType CcpIvdCheck_Validate(
    const uint8 * const calculatedData,
    const uint8 * const expectedData,
    uint32 dataLength)
{
    Std_ReturnType result = STD_NOT_OK;
    uint32 index;
    uint8 difference = (uint8)0U;

    if (CcpIvdCheck_Context.initialized == FALSE)
    {
        CcpIvdCheck_Context.result = CCP_IVD_CHECK_RESULT_ERROR;
    }
    else if ((calculatedData == NULL_PTR) ||
             (expectedData == NULL_PTR) ||
             (dataLength == (uint32)0U))
    {
        CcpIvdCheck_Context.result = CCP_IVD_CHECK_RESULT_ERROR;
    }
    else
    {
        for (index = (uint32)0U; index < dataLength; index++)
        {
            difference = (uint8)(difference |
                (uint8)(calculatedData[index] ^ expectedData[index]));
        }

        CcpIvdCheck_IncrementCheckCount();
        if (difference == (uint8)0U)
        {
            CcpIvdCheck_Context.result = CCP_IVD_CHECK_RESULT_VALID;
            result = STD_OK;
        }
        else
        {
            CcpIvdCheck_Context.result = CCP_IVD_CHECK_RESULT_INVALID;
        }
    }

    return result;
}

/** \brief Returns the module-owned read-only IVD check context. */
const CcpIvdCheck_ContextType * CcpIvdCheck_GetContext(void)
{
    return &CcpIvdCheck_Context;
}

/** \brief Reports whether the current IVD calculation is complete. */
boolean CcpIvdCheck_IsCalculationComplete(void)
{
    return CcpIvdCheck_Context.calculationComplete;
}

/** \brief Returns read-only access to the calculated IVD digest. */
const uint8 * CcpIvdCheck_GetCalculatedData(void)
{
    return &CcpIvdCheck_CalculatedData[0];
}

/** \brief Returns the configured digest length in bytes. */
uint32 CcpIvdCheck_GetCalculatedDataLength(void)
{
    return (uint32)PRJ_IVD_DIGEST_LENGTH;
}

/*******************************************************************************
    10. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Clears the complete calculated digest buffer. */
static void CcpIvdCheck_ClearCalculatedData(void)
{
    uint32 index;

    for (index = (uint32)0U;
         index < (uint32)PRJ_IVD_DIGEST_LENGTH;
         index++)
    {
        CcpIvdCheck_CalculatedData[index] = (uint8)0U;
    }
}

/** \brief Increments the diagnostic counter without unsigned wraparound. */
static void CcpIvdCheck_IncrementCheckCount(void)
{
    if (CcpIvdCheck_Context.checkCount < CCP_IVD_CHECK_COUNT_MAX)
    {
        CcpIvdCheck_Context.checkCount++;
    }
    else
    {
        /* Keep the diagnostic count saturated at its maximum value. */
    }
}

/** \brief Aborts active work and returns the calculation to its idle stage. */
static void CcpIvdCheck_ResetCalculation(void)
{
    CcpIvdCheck_CalculationContext.state = CCP_IVD_CALC_STATE_IDLE;
    CcpIvdCheck_CalculationContext.currentAddress = (uint32)0U;
    CcpIvdCheck_CalculationContext.remainingLength = (uint32)0U;
    CcpIvdCheck_Context.calculationActive = FALSE;
    CcpIvdCheck_Context.calculationElapsedTicks = (uint32)0U;
}

#if (PRJ_IVD_CALCULATION_ENABLE == 1U)
/** \brief Latches a calculation failure for diagnostic observation. */
static void CcpIvdCheck_SetCalculationError(void)
{
    CcpIvdCheck_CalculationContext.state = CCP_IVD_CALC_STATE_ERROR;
    CcpIvdCheck_Context.result = CCP_IVD_CHECK_RESULT_ERROR;
    CcpIvdCheck_Context.calculationActive = FALSE;
    CcpIvdCheck_Context.calculationComplete = FALSE;
    CcpIvdCheck_Context.calculationError = TRUE;
}

/** \brief Executes the operation associated with the current hash stage. */
static void CcpIvdCheck_ProcessCalculation(void)
{
    SecureFlash_RT secureFlashResult;

    switch (CcpIvdCheck_CalculationContext.state)
    {
        case CCP_IVD_CALC_STATE_IDLE:
            CcpIvdCheck_ClearCalculatedData();
            CcpIvdCheck_Context.calculationActive = TRUE;
            CcpIvdCheck_Context.calculationComplete = FALSE;
            CcpIvdCheck_Context.calculationError = FALSE;
            CcpIvdCheck_Context.calculationElapsedTicks = (uint32)0U;
            CcpIvdCheck_CalculationContext.state = CCP_IVD_CALC_STATE_START;
            break;

        case CCP_IVD_CALC_STATE_START:
            secureFlashResult = SecureFlash_HashStart();
            if (secureFlashResult == RT_SUCCESS)
            {
                CcpIvdCheck_SelectSection(
                    (uint32)PRJ_IVD_SECTION1_START_ADDRESS,
                    CCP_IVD_SECTION1_LENGTH,
                    CCP_IVD_CALC_STATE_SECTION1);
            }
            else
            {
                /* Retry a temporarily unavailable service until timeout. */
            }
            break;

        case CCP_IVD_CALC_STATE_SECTION1:
            CcpIvdCheck_ProcessFlashSection(CCP_IVD_CALC_STATE_SECTION2);
            break;

        case CCP_IVD_CALC_STATE_SECTION2:
            CcpIvdCheck_ProcessFlashSection(CCP_IVD_CALC_STATE_FINISH);
            break;

        case CCP_IVD_CALC_STATE_FINISH:
            secureFlashResult = SecureFlash_HashFinish(
                &CcpIvdCheck_CalculatedData[0]);
            if (secureFlashResult == RT_SUCCESS)
            {
                CcpIvdCheck_CalculationContext.state =
                    CCP_IVD_CALC_STATE_COMPLETE;
                CcpIvdCheck_Context.calculationActive = FALSE;
                CcpIvdCheck_Context.calculationComplete = TRUE;
            }
            else
            {
                /* Retry a temporarily unavailable service until timeout. */
            }
            break;

        case CCP_IVD_CALC_STATE_COMPLETE:
            /* The completed digest remains latched until reinitialization. */
            break;

        case CCP_IVD_CALC_STATE_ERROR:
            /* The error remains latched until reinitialization. */
            break;

        default:
            CcpIvdCheck_SetCalculationError();
            break;
    }
}

/** \brief Processes one bounded chunk of the selected flash section. */
static void CcpIvdCheck_ProcessFlashSection(
    CcpIvdCheck_CalculationStateType nextState)
{
    SecureFlash_RT secureFlashResult;
    uint32 calculationLength;
    const uint8 * calculationAddress;

    if (CcpIvdCheck_CalculationContext.remainingLength >
        (uint32)PRJ_IVD_CALCULATION_CHUNK_LENGTH)
    {
        calculationLength = (uint32)PRJ_IVD_CALCULATION_CHUNK_LENGTH;
    }
    else
    {
        calculationLength =
            CcpIvdCheck_CalculationContext.remainingLength;
    }

    if (calculationLength > (uint32)0U)
    {
        calculationAddress = CCP_IVD_ADDRESS_TO_POINTER(
            CcpIvdCheck_CalculationContext.currentAddress);
        secureFlashResult = SecureFlash_HashUpdate(
            calculationAddress,
            calculationLength);

        if (secureFlashResult == RT_SUCCESS)
        {
            CcpIvdCheck_CalculationContext.currentAddress +=
                calculationLength;
            CcpIvdCheck_CalculationContext.remainingLength -=
                calculationLength;

            if (CcpIvdCheck_CalculationContext.remainingLength ==
                (uint32)0U)
            {
                if (nextState == CCP_IVD_CALC_STATE_SECTION2)
                {
                    CcpIvdCheck_SelectSection(
                        (uint32)PRJ_IVD_SECTION2_START_ADDRESS,
                        CCP_IVD_SECTION2_LENGTH,
                        CCP_IVD_CALC_STATE_SECTION2);
                }
                else
                {
                    CcpIvdCheck_CalculationContext.state = nextState;
                }
            }
            else
            {
                /* Continue the current section on the next invocation. */
            }
        }
        else
        {
            /* Retry a temporarily unavailable service until timeout. */
        }
    }
    else
    {
        CcpIvdCheck_SetCalculationError();
    }
}

/** \brief Selects a configured flash range for incremental processing. */
static void CcpIvdCheck_SelectSection(
    uint32 startAddress,
    uint32 sectionLength,
    CcpIvdCheck_CalculationStateType state)
{
    CcpIvdCheck_CalculationContext.currentAddress = startAddress;
    CcpIvdCheck_CalculationContext.remainingLength = sectionLength;
    CcpIvdCheck_CalculationContext.state = state;
}
#endif
