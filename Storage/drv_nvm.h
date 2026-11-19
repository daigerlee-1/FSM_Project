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
    \file   drv_nvm.h
    \brief  Declares the project NvM service abstraction.
            The interface maps logical block identifiers to AUTOSAR RTE NvM
            services and provides a RAM-backed substitute for PC builds.
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
| 2026-11-12 000.01     DL      Added block-based AUTOSAR NvM abstraction
********************************************************************************/
#ifndef DRV_NVM_H
#define DRV_NVM_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Length of each project-specific NvM block in bytes. */
#define DRV_NVM_ACC_CNT_LENGTH          ((uint16)1U)
#define DRV_NVM_AVN_LEARNT_INFO_LENGTH  ((uint16)50U)
#define DRV_NVM_DTC_INFO_LENGTH         ((uint16)8U)
#define DRV_NVM_HW_VERSION_INFO_LENGTH  ((uint16)4U)
#define DRV_NVM_IAU_LEARNT_INFO_LENGTH  ((uint16)50U)
#define DRV_NVM_MDATE_LENGTH            ((uint16)4U)
#define DRV_NVM_NOTI_INFO_LENGTH        ((uint16)6U)
#define DRV_NVM_PART_NO_LENGTH          ((uint16)10U)
#define DRV_NVM_SERIAL_NO_LENGTH        ((uint16)10U)
#define DRV_NVM_SYS_LOG_DATA_LENGTH     ((uint16)52U)
#define DRV_NVM_USER_INFO_LENGTH        ((uint16)8U)

/** Largest configured NvM block; used only by the standalone RAM emulator. */
#define DRV_NVM_MAX_BLOCK_LENGTH (DRV_NVM_SYS_LOG_DATA_LENGTH)

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** Identifies a configured NvM block independently of generated RTE names. */
typedef enum
{
    DRV_NVM_BLOCK_ACC_CNT = 0U,
    DRV_NVM_BLOCK_AVN_LEARNT_INFO,
    DRV_NVM_BLOCK_DTC_INFO,
    DRV_NVM_BLOCK_HW_VERSION_INFO,
    DRV_NVM_BLOCK_IAU_LEARNT_INFO,
    DRV_NVM_BLOCK_MDATE,
    DRV_NVM_BLOCK_NOTI_INFO,
    DRV_NVM_BLOCK_PART_NO,
    DRV_NVM_BLOCK_SERIAL_NO,
    DRV_NVM_BLOCK_SYS_LOG_DATA,
    DRV_NVM_BLOCK_USER_INFO,
    DRV_NVM_BLOCK_MAX
} DrvNvm_BlockIdType;

/** Normalized asynchronous NvM request result returned to the application. */
typedef enum
{
    DRV_NVM_REQUEST_OK = 0U,
    DRV_NVM_REQUEST_PENDING,
    DRV_NVM_REQUEST_FAILED
} DrvNvm_RequestResultType;

/*******************************************************************************
    7. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/**
 * \brief Initializes the NvM abstraction.
 *
 * AUTOSAR initialization is owned by BSW. The standalone implementation
 * clears its RAM-backed test storage and request status values.
 */
void DrvNvm_Init(void);

/**
 * \brief Returns the configured byte length of a logical NvM block.
 * \param[in] blockId Logical NvM block identifier.
 * \return Configured block length, or zero for an invalid identifier.
 */
uint16 DrvNvm_GetBlockLength(DrvNvm_BlockIdType blockId);

/**
 * \brief Requests an asynchronous read of one NvM block.
 * \param[in] blockId Logical NvM block identifier.
 * \param[out] data Destination buffer retained by the caller until completion.
 * \param[in] length Destination length; it must match the configured block.
 * \return STD_OK when the request is accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType DrvNvm_Read(
    DrvNvm_BlockIdType blockId,
    uint8 * const data,
    uint16 length);

/**
 * \brief Requests an asynchronous write of one NvM block.
 * \param[in] blockId Logical NvM block identifier.
 * \param[in] data Source buffer retained by the caller until completion.
 * \param[in] length Source length; it must match the configured block.
 * \return STD_OK when the request is accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType DrvNvm_Write(
    DrvNvm_BlockIdType blockId,
    const uint8 * const data,
    uint16 length);

/**
 * \brief Requests erasure of one NvM block.
 * \param[in] blockId Logical NvM block identifier.
 * \return STD_OK when the request is accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType DrvNvm_Erase(DrvNvm_BlockIdType blockId);

/**
 * \brief Obtains the normalized asynchronous status of an NvM block request.
 * \param[in] blockId Logical NvM block identifier.
 * \param[out] requestResult Normalized request result destination.
 * \return STD_OK when the status was obtained; otherwise STD_NOT_OK.
 */
Std_ReturnType DrvNvm_GetErrorStatus(
    DrvNvm_BlockIdType blockId,
    DrvNvm_RequestResultType * const requestResult);

#endif /* DRV_NVM_H */
