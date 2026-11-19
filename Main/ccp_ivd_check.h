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
    \file   ccp_ivd_check.h
    \brief  Declares the CCP Integrity Validation Data check interface.
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
| 2026-11-13 000.01     DL      Creation
| 2026-11-13 000.02     DL      Added nonblocking SecureFlash calculation
********************************************************************************/
#ifndef CCP_IVD_CHECK_H
#define CCP_IVD_CHECK_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"
#include "prj_mcu_ivd.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Identifies the latest Integrity Validation Data check result. */
typedef enum
{
    CCP_IVD_CHECK_RESULT_NOT_EXECUTED = 0U,
    CCP_IVD_CHECK_RESULT_VALID,
    CCP_IVD_CHECK_RESULT_INVALID,
    CCP_IVD_CHECK_RESULT_ERROR,
    CCP_IVD_CHECK_RESULT_MAX
} CcpIvdCheck_ResultType;

/** Contains the observable runtime state of the IVD check module. */
typedef struct
{
    CcpIvdCheck_ResultType result;
    boolean initialized;
    boolean calculationActive;
    boolean calculationComplete;
    boolean calculationError;
    uint32 calculationElapsedTicks;
    uint32 checkCount;
} CcpIvdCheck_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/** Initializes the IVD check context. */
void CcpIvdCheck_Init(void);

/**
 * \brief Advances the nonblocking IVD calculation by one 5 ms step.
 *
 * At most PRJ_IVD_CALCULATION_CHUNK_LENGTH flash bytes are supplied to the
 * SecureFlash hash service during one invocation.
 */
void CcpIvdCheck_MainFunction5ms(void);

/**
 * \brief Compares calculated and expected Integrity Validation Data.
 *
 * The complete buffers are compared without terminating at the first mismatch.
 * Both pointers shall address at least dataLength readable bytes.
 *
 * \param[in] calculatedData Calculated or received IVD byte sequence.
 * \param[in] expectedData   Reference IVD byte sequence.
 * \param[in] dataLength Number of bytes to compare; shall be greater than
 *                       zero.
 * \return STD_OK when both buffers match; otherwise STD_NOT_OK.
 */
Std_ReturnType CcpIvdCheck_Validate(
    const uint8 * const calculatedData,
    const uint8 * const expectedData,
    uint32 dataLength);

/**
 * \brief Returns read-only access to the IVD check context.
 * \return Pointer to the module-owned context.
 */
const CcpIvdCheck_ContextType * CcpIvdCheck_GetContext(void);

/**
 * \brief Reports whether the current IVD calculation completed successfully.
 * \return TRUE after hash calculation completes; otherwise FALSE.
 */
boolean CcpIvdCheck_IsCalculationComplete(void);

/** Returns read-only access to the calculated IVD digest. */
const uint8 * CcpIvdCheck_GetCalculatedData(void);

/** Returns the configured calculated IVD digest length in bytes. */
uint32 CcpIvdCheck_GetCalculatedDataLength(void);

#endif /* CCP_IVD_CHECK_H */
