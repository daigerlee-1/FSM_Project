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
    \file   prj_mcu_ivd.h
    \brief  Declares CYT4BB-specific Integrity Validation Data configuration.
*******************************************************************************/
#ifndef PRJ_MCU_IVD_H
#define PRJ_MCU_IVD_H

/*******************************************************************************
    3. HEADER FILES
*******************************************************************************/
#include "prj_defs.h"

/*******************************************************************************
    4. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Enables target SecureFlash IVD calculation. */
#ifndef PRJ_IVD_CALCULATION_ENABLE
    #if (PRJ_MOBILGENE_ENABLE == 1U)
    #define PRJ_IVD_CALCULATION_ENABLE (1U)
    #else
    #define PRJ_IVD_CALCULATION_ENABLE (0U)
    #endif
#endif

/** First byte included in IVD signature section 1. */
#define PRJ_IVD_SECTION1_START_ADDRESS (0x10058000U)

/** Last byte included in IVD signature section 1. */
#define PRJ_IVD_SECTION1_END_ADDRESS   (0x100588FFU)

/** First byte included in IVD signature section 2. */
#define PRJ_IVD_SECTION2_START_ADDRESS (0x10058C00U)

/** Last byte included in IVD signature section 2. */
#define PRJ_IVD_SECTION2_END_ADDRESS   (0x101F7FFFU)

/** Maximum number of flash bytes processed by one 5 ms invocation. */
#define PRJ_IVD_CALCULATION_CHUNK_LENGTH (0x100U)

/** Period of CcpIvdCheck_MainFunction5ms in milliseconds. */
#define PRJ_IVD_MAIN_PERIOD_MS (5U)

/** Maximum permitted duration of one IVD calculation in milliseconds. */
#define PRJ_IVD_CALCULATION_TIMEOUT_MS (60000U)

/** Maximum number of 5 ms invocations permitted for one calculation. */
#define PRJ_IVD_CALCULATION_TIMEOUT_TICKS \
    (PRJ_IVD_CALCULATION_TIMEOUT_MS / PRJ_IVD_MAIN_PERIOD_MS)

/** SHA-256 digest length produced by the SecureFlash service. */
#define PRJ_IVD_DIGEST_LENGTH (32U)

/*******************************************************************************
    5. CONFIGURATION VALIDATION
*******************************************************************************/
#if ((PRJ_IVD_CALCULATION_ENABLE != 0U) && \
     (PRJ_IVD_CALCULATION_ENABLE != 1U))
    #error "PRJ_IVD_CALCULATION_ENABLE must be 0U or 1U"
#endif

#if (PRJ_IVD_SECTION1_END_ADDRESS < PRJ_IVD_SECTION1_START_ADDRESS)
    #error "IVD section 1 end address precedes its start address"
#endif

#if (PRJ_IVD_SECTION2_END_ADDRESS < PRJ_IVD_SECTION2_START_ADDRESS)
    #error "IVD section 2 end address precedes its start address"
#endif

#if (PRJ_IVD_SECTION1_END_ADDRESS >= PRJ_IVD_SECTION2_START_ADDRESS)
    #error "Configured IVD signature sections overlap"
#endif

#if (PRJ_IVD_CALCULATION_CHUNK_LENGTH == 0U)
    #error "PRJ_IVD_CALCULATION_CHUNK_LENGTH must be greater than zero"
#endif

#if (PRJ_IVD_MAIN_PERIOD_MS == 0U)
    #error "PRJ_IVD_MAIN_PERIOD_MS must be greater than zero"
#else
    #if (PRJ_IVD_CALCULATION_TIMEOUT_MS < PRJ_IVD_MAIN_PERIOD_MS)
        #error "IVD calculation timeout is shorter than its main period"
    #endif

    #if ((PRJ_IVD_CALCULATION_TIMEOUT_MS % PRJ_IVD_MAIN_PERIOD_MS) != 0U)
        #error "IVD calculation timeout must be divisible by its main period"
    #endif
#endif

#if (PRJ_IVD_DIGEST_LENGTH == 0U)
    #error "PRJ_IVD_DIGEST_LENGTH must be greater than zero"
#endif

#endif /* PRJ_MCU_IVD_H */
