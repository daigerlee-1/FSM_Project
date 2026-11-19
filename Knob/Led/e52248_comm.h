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
    \file   e52248_comm.h
    \brief  Declares the asynchronous E52248 I2C communication interface.
            This interface provides queued read and write requests, completion
            notifications, job results, and read-only diagnostic context data.
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
| 2026-10-15 000.02     DL      Standardized public header documentation
| 2026-10-20 000.03     DL      Added completed-read data copy interface
********************************************************************************/
#ifndef E52248_COMM_H
#define E52248_COMM_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_defs.h"
#include "prj_typedefs.h"

#include "fsm.h"
#include "e52248_register.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/* No public definitions or macros are declared by this module. */

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** \brief Identifies the I2C hardware channel assigned to the E52248 device. */
typedef enum
{
    E52248_I2C_CHANNEL_SCB3 = 0U,
    E52248_I2C_CHANNEL_SCB7,
    E52248_I2C_CHANNEL_MAX
} E52248_I2cChannelType;

/** \brief Identifies the direction of the active E52248 communication job. */
typedef enum
{
    E_WRITE = 0U,
    E_READ
} E52248_DirType;

/** \brief Identifies the externally observable E52248 communication status. */
typedef enum
{
    E_COMM_STS_IDLE = 0U,
    E_COMM_STS_BUSY,
    E_COMM_STS_ERR_COM,
    E_COMM_STS_ERR_CRC,
    E_COMM_STS_ERR_PAGE,
    E_COMM_STS_ERR_PRGSTS,
    E_COMM_STS_MAX
} E52248_CommState;

/** \brief Identifies the result of an asynchronous E52248 communication job. */
typedef enum
{
    E52248_JOB_RESULT_NONE = 0U,
    E52248_JOB_RESULT_PENDING,
    E52248_JOB_RESULT_READ_OK,
    E52248_JOB_RESULT_WRITE_OK,
    E52248_JOB_RESULT_ERROR_COM,
    E52248_JOB_RESULT_ERROR_CRC
} E52248_JobResultType;

/** \brief Identifies the number of 10-bit words transferred by one job. */
typedef enum
{
    E_SIZE_TYPE_1 = 0U,
    E_SIZE_TYPE_4,
    E_SIZE_TYPE_16,
    E_SIZE_TYPE_MAX
} E52248_CommSizeType;

/** \brief Contains the observable E52248 communication runtime data. */
typedef struct
{
    /** Selected I2C hardware channel. */
    E52248_I2cChannelType i2c_ch;

    /** Asynchronous hardware notification flags. */
    volatile uint8      i2c_notiRx;
    volatile uint8      i2c_notiTx;
    volatile uint8      i2c_notiErr;

    /** Active job addressing, direction, status, and result information. */
    uint8               i2c_addr;
    uint8               reg_addr;
    E52248_DirType      dir;
    E52248_CommState    comm_sts;
    E52248_CommSizeType size_type;
    E52248_JobResultType job_result;

    /** Receive data sizes and buffer references. */
    uint8               rx_size;
    uint16              *rx_buf;
    uint8               *rx_raw_buf;

    /** Transmit data size and frame-buffer reference. */
    uint8               tx_size;
    uint8               *tx_buf;

    /** Communication payload and LED color-buffer references. */
    uint16              *data_buf;
    uint16              *rgb_buf;

    /** LED update and communication retry counters. */
    uint8               light_changed;
    uint8               retry_cnt;
    uint8               retry_otp_prog_cnt;

    Fsm_ContextType     fsm;
} E52248_ContextType;

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Initializes the E52248 communication context and internal FSM.
 *
 * \return STD_OK when initialization succeeds; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_Init(void);

/**
 * \brief Executes the 5 ms E52248 asynchronous communication processing.
 */
void E52248_Comm_MainFunction5ms(void);

/**
 * \brief Requests an asynchronous E52248 register read.
 *
 * \param[in] slaveAddress    I2C slave address of the E52248 device.
 * \param[in] registerAddress First register address to read.
 * \param[in] sizeType        Number of 10-bit words to read.
 *
 * \return STD_OK when the request is queued; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_StartRead(uint8 slaveAddress,
                                     uint8 registerAddress,
                                     E52248_CommSizeType sizeType);

/**
 * \brief Requests an asynchronous E52248 register write.
 *
 * The payload is copied into the communication module before this function
 * returns.
 *
 * \param[in] slaveAddress    I2C slave address of the E52248 device.
 * \param[in] registerAddress First register address to write.
 * \param[in] sizeType        Number of 10-bit words to write.
 * \param[in] data            Source payload containing the requested words.
 *
 * \return STD_OK when the request is queued; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_StartWrite(uint8 slaveAddress,
                                      uint8 registerAddress,
                                      E52248_CommSizeType sizeType,
                                      const uint16 *data);

/**
 * \brief Retrieves and consumes the latest completed communication result.
 *
 * \param[out] result Destination for the completed job result.
 *
 * \return STD_OK when a completed result was returned; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_TakeResult(
    E52248_JobResultType * const result);

/**
 * \brief Copies the converted words from the latest read transaction.
 *
 * The requested word count shall exactly match the latest read size.
 *
 * \param[out] data      Destination for converted 10-bit register words.
 * \param[in]  wordCount Number of destination words available.
 * \return STD_OK when copied successfully; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_CopyReadData(uint16 * const data,
                                        uint8 wordCount);

/** Records an asynchronous I2C receive-complete notification. */
void E52248_Comm_RxNotification(void);

/** Records an asynchronous I2C transmit-complete notification. */
void E52248_Comm_TxNotification(void);

/** Records an asynchronous I2C communication-error notification. */
void E52248_Comm_ErrorNotification(void);

/**
 * \brief Returns a read-only diagnostic view of the communication context.
 *
 * \return Pointer to the statically allocated E52248 communication context.
 */
const E52248_ContextType * E52248_Comm_GetContext(void);

#endif /* E52248_COMM_H */
