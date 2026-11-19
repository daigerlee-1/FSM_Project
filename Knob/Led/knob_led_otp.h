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
    \file   knob_led_otp.h
    \brief  Declares internal E52248 OTP sequence services.
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
| 2026-11-09 000.02     DL      Added selectable OTP programming address
********************************************************************************/
#ifndef KNOB_LED_OTP_H
#define KNOB_LED_OTP_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_defs.h"
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Identifies the result of advancing the asynchronous OTP write sequence. */
typedef enum
{
    LED_OTP_SEQUENCE_PENDING = 0U,
    LED_OTP_SEQUENCE_COMPLETE,
    LED_OTP_SEQUENCE_INVALID,
    LED_OTP_SEQUENCE_ERROR
} LedOtp_SequenceResultType;

/** Contains the latest OTP address-and-value validation result. */
typedef struct
{
    boolean valid;
    uint8 configIndex;
    uint16 expectedValue;
    uint16 actualValue;
} LedOtp_ValidationResultType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Starts writing the OTP configuration one register word at a time.
 *
 * \param[in] slaveAddress E52248 address used for the complete write sequence.
 * \return STD_OK when the first write is queued; otherwise STD_NOT_OK.
 */
Std_ReturnType LedOtp_StartWrite(uint8 slaveAddress);

/**
 * \brief Advances the OTP sequence after a successful communication job.
 *
 * \return Current state of the OTP write sequence.
 */
LedOtp_SequenceResultType LedOtp_ContinueWrite(void);

/** Cancels the active OTP write sequence after a communication error. */
void LedOtp_AbortWrite(void);

/**
 * \brief Starts validating every configured OTP address and value.
 *
 * \param[in] slaveAddress Active E52248 I2C slave address.
 * \return STD_OK when the first page-select write is queued.
 */
Std_ReturnType LedOtp_StartValidityCheck(uint8 slaveAddress);

/** Advances validation after a successful page-select write or data read. */
LedOtp_SequenceResultType LedOtp_ContinueValidityCheck(void);

/** Cancels an active OTP validation sequence after a communication error. */
void LedOtp_AbortValidityCheck(void);

/** Returns the latest OTP validation result for diagnostic observation. */
const LedOtp_ValidationResultType * LedOtp_GetValidationResult(void);

#endif /* KNOB_LED_OTP_H */
