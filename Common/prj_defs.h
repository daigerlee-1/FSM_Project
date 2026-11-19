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
    \file   prj_defs.h
    \brief  Declares project-wide compile-time configuration definitions.
            This header selects the vehicle and version format, target
            environment, hardware channels, and optional test features.
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
| 2026-11-05 000.03     DL      Added E52248 I2C channel selection
| 2026-11-05 000.04     DL      Centralized configuration validation
| 2026-11-06 000.05     DL      Added optional LCAN output controls
| 2026-11-08 000.06     DL      Added guarded LED OTP test-write control
| 2026-11-09 000.07     DL      Added optional LED Open/Short diagnostic test
| 2026-11-09 000.08     DL      Added JX2 vehicle-project selection
| 2026-11-10 000.09     DL      Added validated JX2 CCP version-format selection
| 2026-11-12 000.10     DL      Added vehicle powertrain-type selection
********************************************************************************/
#ifndef PRJ_DEFS_H
#define PRJ_DEFS_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
/* This project-configuration header has no include dependencies. */

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/**
 * \brief Selects the target software environment.
 *
 * This snapshot contains only the Mobilgene/AUTOSAR target.
 */

/**
 * \brief Identifies the host operating system for standalone builds.
 *
 * Both platform macros are 0U for Mobilgene/AUTOSAR target builds.
 */
    #define PLATFORM_WINDOWS (0U)
    #define PLATFORM_LINUX   (0U)


/**
 * \brief Selects diagnostic and identification data for the JX2 vehicle.
 *
 * Set to 1U for the JX2 project or 0U when another vehicle-data set is
 * selected by the build configuration.
 */
#ifndef VEHICLE_JX2_PRJ
#define VEHICLE_JX2_PRJ (1U)
#endif

/** Internal-combustion-engine vehicle powertrain selection value. */
#define VEHICLE_ICE (0U)

/** Hybrid-electric vehicle powertrain selection value. */
#define VEHICLE_HEV (1U)

/** Battery-electric vehicle powertrain selection value. */
#define VEHICLE_EV  (2U)

/* CCP software-version formats are configured only for the JX2 project. */
#if (VEHICLE_JX2_PRJ == 1U)

    /**
     * \brief Selects the powertrain signal used for the vehicle-ready condition.
     *
     * Select exactly one of VEHICLE_ICE, VEHICLE_HEV, or VEHICLE_EV. The selected
     * type determines whether EngSta, HevRdySta, or EvDrvRdySta is evaluated.
     */
    #ifndef VEHICLE_POWERTRAIN_TYPE
    #define VEHICLE_POWERTRAIN_TYPE (VEHICLE_EV)
    #endif

    /**
     * \brief Enables the JX2 diagnostic master-key table.
     *
     * Set to 1U to provide MasterKeyTable when diagnostic-service definitions are
     * enabled.
     */
    #ifndef CCP_MASTERKEY_ENABLE
    #define CCP_MASTERKEY_ENABLE (1U)
    #endif

    /** Selects the 41-byte JX2 CCP software-version format. */
    #ifndef CCP_VERSION_41BYTE
    #define CCP_VERSION_41BYTE (1U)
    #endif

    /** Selects the complementary 10-byte JX2 CCP software-version format. */
    #ifndef CCP_VERSION_10BYTE
        #if (CCP_VERSION_41BYTE == 1U)
        #define CCP_VERSION_10BYTE (0U)
        #else
        #define CCP_VERSION_10BYTE (1U)
        #endif
    #endif

#else

    /** This causes an error because the CCP version format has not been set.  */
    #ifndef CCP_VERSION_41BYTE
    #define CCP_VERSION_41BYTE (0U)
    #endif
    #ifndef CCP_VERSION_10BYTE
    #define CCP_VERSION_10BYTE (0U)
    #endif

#endif /* VEHICLE_JX2_PRJ == 1U */

/**
 * \brief Selects the I2C hardware channel used by the E52248 LED controller.
 *
 * Set to 1U to use SCB3 or 0U to use SCB7. The selection is applied to the
 * reset GPIO route, I2C initialization, read/write requests, and communication
 * context channel. The build system may override this default definition.
 */
#ifndef I2C_SCB3_ENABLE
#define I2C_SCB3_ENABLE (1U)
#endif

#ifndef I2C_LED_TEST_ENABLE
#define I2C_LED_TEST_ENABLE         (0U) 
#endif

/**
 * \brief Enables the runtime LED OTP test-write request when set to 1U.
 *
 * Keep this setting at 0U for production builds. When enabled, the LED
 * application may enter OTP programming mode from LED_STATE_NORMAL through
 * the explicit test request API. Only one request is accepted per boot.
 */
#ifndef PRJ_LED_OTP_TEST_WRITE_ENABLE
#define PRJ_LED_OTP_TEST_WRITE_ENABLE (1U)
#endif

/**
 * \brief Enables the LED Open/Short status-register test when set to 1U.
 *
 * The test reads the four consecutive E52248 registers from 0xB8 through
 * 0xBB while the LED application is in LED_STATE_NORMAL. Keep disabled in
 * production builds when this Trace32/testcase visibility is not required.
 */
#ifndef PRJ_LED_OPEN_SHORT_TEST_ENABLE
#define PRJ_LED_OPEN_SHORT_TEST_ENABLE (1U)
#endif


/**
 * \brief Enables LCAN Touch RTE output writes when set to 1U.
 *
 * Keep disabled while Touch is unsupported because each Rte_Write call can
 * trigger transmission for ports configured with OnWrite transmission.
 */
#ifndef PRJ_LCAN_TOUCH_OUTPUT_ENABLE
#define PRJ_LCAN_TOUCH_OUTPUT_ENABLE (0U)
#endif

/**
 * \brief Enables LCAN LOG RTE output writes when set to 1U.
 *
 * Keep disabled while LOG output is unsupported so that the related RTE calls
 * and fallback values are not included in the compiled software.
 */
#ifndef PRJ_LCAN_LOG_OUTPUT_ENABLE
#define PRJ_LCAN_LOG_OUTPUT_ENABLE (0U)
#endif

/*******************************************************************************
    6. CONFIGURATION VALIDATION
*******************************************************************************/
/** Reject unsupported binary configuration values at preprocessing time. */

#if ((VEHICLE_JX2_PRJ != 0U) && (VEHICLE_JX2_PRJ != 1U))
    #error "VEHICLE_JX2_PRJ must be 0U or 1U"
#endif

#ifndef VEHICLE_POWERTRAIN_TYPE
    #error "VEHICLE_POWERTRAIN_TYPE must be explicitly configured"
#endif

#if ((VEHICLE_POWERTRAIN_TYPE != VEHICLE_ICE) && \
     (VEHICLE_POWERTRAIN_TYPE != VEHICLE_HEV) && \
     (VEHICLE_POWERTRAIN_TYPE != VEHICLE_EV))
    #error "VEHICLE_POWERTRAIN_TYPE must select ICE, HEV, or EV"
#endif

#ifndef CCP_MASTERKEY_ENABLE
    #error "CCP_MASTERKEY_ENABLE must be explicitly configured"
#endif

#if ((CCP_VERSION_41BYTE != 0U) && (CCP_VERSION_41BYTE != 1U))
    #error "CCP_VERSION_41BYTE must be 0U or 1U"
#endif

#if ((CCP_VERSION_10BYTE != 0U) && (CCP_VERSION_10BYTE != 1U))
    #error "CCP_VERSION_10BYTE must be 0U or 1U"
#endif

#if ((CCP_MASTERKEY_ENABLE != 0U) && (CCP_MASTERKEY_ENABLE != 1U))
    #error "CCP_MASTERKEY_ENABLE must be 0U or 1U"
#endif

#if (VEHICLE_JX2_PRJ == 1U)
    #if ((CCP_VERSION_41BYTE + CCP_VERSION_10BYTE) != 1U)
        #error "Exactly one JX2 CCP version format must be enabled"
    #endif
#else
    #if ((CCP_VERSION_41BYTE != 0U) || (CCP_VERSION_10BYTE != 0U))
        #error "CCP version formats require VEHICLE_JX2_PRJ"
    #endif
#endif

#if ((I2C_SCB3_ENABLE != 0U) && (I2C_SCB3_ENABLE != 1U))
    #error "I2C_SCB3_ENABLE must be 0U or 1U"
#endif

#if ((PRJ_LED_OTP_TEST_WRITE_ENABLE != 0U) && \
     (PRJ_LED_OTP_TEST_WRITE_ENABLE != 1U))
    #error "PRJ_LED_OTP_TEST_WRITE_ENABLE must be 0U or 1U"
#endif

#if ((PRJ_LED_OPEN_SHORT_TEST_ENABLE != 0U) && \
     (PRJ_LED_OPEN_SHORT_TEST_ENABLE != 1U))
    #error "PRJ_LED_OPEN_SHORT_TEST_ENABLE must be 0U or 1U"
#endif

#if ((PRJ_LCAN_TOUCH_OUTPUT_ENABLE != 0U) && \
     (PRJ_LCAN_TOUCH_OUTPUT_ENABLE != 1U))
    #error "PRJ_LCAN_TOUCH_OUTPUT_ENABLE must be 0U or 1U"
#endif

#if ((PRJ_LCAN_LOG_OUTPUT_ENABLE != 0U) && \
     (PRJ_LCAN_LOG_OUTPUT_ENABLE != 1U))
    #error "PRJ_LCAN_LOG_OUTPUT_ENABLE must be 0U or 1U"
#endif

/*******************************************************************************
    7. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No public data types are declared by this module. */

/*******************************************************************************
    8. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    9. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/* No public functions are declared by this module. */

#endif /* PRJ_DEFS_H */
