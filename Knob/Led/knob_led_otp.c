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
    \file   knob_led_otp.c
    \brief  Implements asynchronous E52248 OTP write and validation sequences.
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
| 2026-10-20 000.02     DL      Documented OTP configuration table
| 2026-10-20 000.03     DL      Documented DIAG0 and DIAG1 channel routing
| 2026-11-05 000.04     DL      Added address-and-value OTP validation
| 2026-11-09 000.05     DL      Added selectable OTP programming address
| 2026-11-09 000.06     DL      Documented calibrated diagnostic thresholds
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "knob_led_otp.h"
#include "e52248_comm.h"
#include "e52248_register.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Maximum E52248 register-word value supported by the protocol. */
#define LED_OTP_DATA_MAX ((uint16)0x3FFU)

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
typedef struct
{
    uint8 page;
    uint8 address;
    uint16 data;
} LedOtp_ConfigType;

/** Identifies which transaction of one OTP table entry is active. */
typedef enum
{
    LED_OTP_PHASE_IDLE = 0U,
    LED_OTP_PHASE_PAGE_SELECT,
    LED_OTP_PHASE_DATA_WRITE
} LedOtp_PhaseType;

/** Identifies the active transaction of the OTP validation sequence. */
typedef enum
{
    LED_OTP_VALIDATION_PHASE_IDLE = 0U,
    LED_OTP_VALIDATION_PHASE_PAGE_SELECT,
    LED_OTP_VALIDATION_PHASE_DATA_READ
} LedOtp_ValidationPhaseType;

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
static Std_ReturnType LedOtp_QueuePageSelect(uint8 slaveAddress,
                                             uint8 configIndex);
static Std_ReturnType LedOtp_QueueDataWrite(void);
static Std_ReturnType LedOtp_QueueValidationRead(void);
static LedOtp_SequenceResultType LedOtp_ContinueValidationPageSelect(void);
static LedOtp_SequenceResultType LedOtp_ContinueValidationDataRead(void);
static boolean LedOtp_IsConfigValid(const LedOtp_ConfigType *config);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Index and phase of the active asynchronous OTP write sequence. */
static uint8 LedOtp_ConfigIndex;
static uint8 LedOtp_WriteSlaveAddress;
static LedOtp_PhaseType LedOtp_WritePhase;
static boolean LedOtp_WriteActive;

/** Runtime state of the asynchronous OTP validation sequence. */
static uint8 LedOtp_ValidationIndex;
static uint8 LedOtp_ValidationSlaveAddress;
static LedOtp_ValidationPhaseType LedOtp_ValidationPhase;
static boolean LedOtp_ValidationActive;
static LedOtp_ValidationResultType LedOtp_ValidationResult;

/** Stable payload storage used while a communication job is queued. */
static uint16 LedOtp_TransferData;

/*******************************************************************************
    9. PRIVATE CONSTANTS
*******************************************************************************/
/**
 * \brief Ordered E52248 OTP register configuration.
 *
 * The sequencer selects the specified OTP page and then writes one register
 * for each entry. Array order therefore defines the programming order.
 */
#if 0
static const LedOtp_ConfigType LedOtp_ConfigData[] =
{
#if 1
    /* Page 1: PWM configuration. */
    {OTP_PAGE_1, OTP_PWM_PRESCALER, 77U},           /* PWM prescaler: 100 Hz. */
    {OTP_PAGE_1, OTP_PWM_PERIOD, 1023U},            /* Define PWM period */

    /* Page 1: LED open diagnostics. */
    {OTP_PAGE_1, OTP_LED_OPEN_THR_1, 12U * 36U},    /* VLED(LEDx) based open detection threshold : 12V=432 (dummy) */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_0_3, 0x50U},      /* Use threshold 1 */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_4_7, 0x51U},      /* Use threshold 1 */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_8_11, 0x51U},     /* Use threshold 1 */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_12_15, 0x01U},    /* Use threshold 1 */

    /* Page 1: LED short diagnostics. */
    {OTP_PAGE_1, OTP_LED_SHORT_THR_1, 12U * 36U},   /* VDIF(LED drop) based short detection threshold : 3V(dummy) */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_0_3, 0x50U},     /* Use threshold 1 */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_4_7, 0x51U},     /* Use threshold 1 */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_8_11, 0x51U},    /* Use threshold 1 */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_12_15, 0x01U},   /* Use threshold 1 */

    /* Page 1: protection and communication. */
    {OTP_PAGE_1, OTP_ILED_OPEN_THR, 100U * 10U},    /* ILED based open detection threshold : 100mA (dummy) */
    {OTP_PAGE_1, OTP_VT_CRITICAL, 125U + 273U},     /* Critical temperature: 125 C. */
    {OTP_PAGE_1, OTP_COM_TIMEOUT, 0x00U},           /* Disable BUS timeout */

    /* Page 2: thermal derating. */
    {OTP_PAGE_2, OTP_VT_DERATE_START, 100U + 273U}, /* Derating starts at 100 C. */
    {OTP_PAGE_2, OTP_VT_DERATE_STOP, 124U + 273U},  /* Derating stops at 124 C. */
    {OTP_PAGE_2, OTP_DERATE_GAIN, (8U << 5U)},      /* VT: 3.125%/°C, VS: 0 */

    /* Page 2: diagnostics and channel configuration. */
    {OTP_PAGE_2, OTP_DIAG_CONFIG, 0x05U},           /* DIAG pin is used, single lamp mode, counter level 3 */
    {OTP_PAGE_2, OTP_ISINK_BUNDLE, 0x00U},         /* channel bundling */

    /* Page 2: LED supply selection. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_0_3, 0x00U},
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_4_7, 0x00U},
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_8_11, 0x00U},
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_12_15, 0x00U},

    /* Page 3: enable CH2 through CH9. */
    {OTP_PAGE_3, OTP_LED_ENABLE_0_7, 0xFCU},        /* OTP Mode Channel Setting */
    {OTP_PAGE_3, OTP_LED_ENABLE_8_15, 0x03U},       /* OTP Mode Channel Setting */

    /* Device address and customer validation value. */
    {OTP_PAGE_1, OTP_COM_DEV_ADDR, I2C_ADDR_VALID},
    {OTP_PAGE_2, OTP_FOR_CUSTOMER_1, I2C_ADDR_VALID}
#else
    /* Page 1: PWM, LED diagnostics, thermal limit, and communication. */
    {OTP_PAGE_1, OTP_PWM_PRESCALER, 25U},          /* PWM prescaler: 300 Hz. */
    {OTP_PAGE_1, OTP_PWM_PERIOD, 1023U},           /* 10-bit PWM period. */
    {OTP_PAGE_1, OTP_LED_OPEN_THR_1, 12U * 36U},   /* Open threshold 1: 12 V. */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_0_3, 0x50U},     /* CH0-CH3 open selectors. */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_4_7, 0x51U},     /* CH4-CH7 open selectors. */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_8_11, 0x51U},    /* CH8-CH11 open selectors. */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_12_15, 0x01U},   /* CH12-CH15 open selectors. */
    {OTP_PAGE_1, OTP_LED_SHORT_THR_1, 12U * 36U},  /* Short threshold 1: 432. */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_0_3, 0x50U},    /* CH0-CH3 short selectors. */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_4_7, 0x51U},    /* CH4-CH7 short selectors. */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_8_11, 0x51U},   /* CH8-CH11 short selectors. */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_12_15, 0x01U},  /* CH12-CH15 short selectors. */
    {OTP_PAGE_1, OTP_ILED_OPEN_THR, 100U * 10U},   /* Open-current threshold: 100 mA. */
    {OTP_PAGE_1, OTP_VT_CRITICAL, 125U + 273U},    /* Critical temperature: 125 C. */
    {OTP_PAGE_1, OTP_COM_TIMEOUT, 0x00U},          /* Bus timeout disabled. */

    /* Page 2: current references, thermal derating, and diagnostics. */
    {OTP_PAGE_2, OTP_IDAC_REF_SEL_0_7, 0xFCU},     /* Select IDAC reference for CH2-CH7. */
    {OTP_PAGE_2, OTP_IDAC_REF_SEL_8_15, 0x03U},    /* Select IDAC reference for CH8-CH9. */
    {OTP_PAGE_2, OTP_VT_DERATE_START, 100U + 273U},/* Derating starts at 100 C. */
    {OTP_PAGE_2, OTP_VT_DERATE_STOP, 124U + 273U}, /* Derating stops at 124 C. */
    {OTP_PAGE_2, OTP_DERATE_GAIN, (8U << 5U)},     /* Encoded derating gain: 8. */
    {OTP_PAGE_2, OTP_DIAG_CONFIG, 0xC5U},          /* Enable configured DIAG outputs. */
    {OTP_PAGE_2, OTP_DIAG0_CONFIG_0_7, 0xFCU},     /* Route CH2-CH7 to DIAG0. */
    {OTP_PAGE_2, OTP_DIAG0_CONFIG_8_15, 0x00U},    /* No CH8-CH15 routing to DIAG0. */
    {OTP_PAGE_2, OTP_DIAG1_CONFIG_0_7, 0x00U},     /* No CH0-CH7 routing to DIAG1. */
    {OTP_PAGE_2, OTP_DIAG1_CONFIG_8_15, 0x03U},    /* Route CH8-CH9 to DIAG1. */
    {OTP_PAGE_2, OTP_ISINK_BUNDLE, 0x10U},         /* Analog bundle: CH8 and CH9. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_0_3, 0x00U},   /* CH0-CH3 supply selectors. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_4_7, 0x00U},   /* CH4-CH7 supply selectors. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_8_11, 0x00U},  /* CH8-CH11 supply selectors. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_12_15, 0x00U}, /* CH12-CH15 supply selectors. */

    /* Page 3: channels enabled after OTP loading. */
    {OTP_PAGE_3, OTP_LED_ENABLE_0_7, 0xFCU},       /* Enable CH2-CH7. */
    {OTP_PAGE_3, OTP_LED_ENABLE_8_15, 0x03U},      /* Enable CH8-CH9. */

    /* Program and retain the normal-operation I2C address. */
    {OTP_PAGE_1, OTP_COM_DEV_ADDR, I2C_ADDR_VALID},
    {OTP_PAGE_2, OTP_FOR_CUSTOMER_1, I2C_ADDR_VALID}
#endif
};
#else
static const LedOtp_ConfigType LedOtp_ConfigData[] =
{
    /* Page 1: PWM configuration. */
    {OTP_PAGE_1, OTP_PWM_PRESCALER, 25U},          /* PWM frequency: 300 Hz. */
    {OTP_PAGE_1, OTP_PWM_PERIOD, 1023U},           /* PWM resolution: 10-bit. */
    {OTP_PAGE_1, OTP_PWM_COMBINE_PRI, 0x10U},      /* Combine PWM control for CH8 and CH9. */

    /* Page 1: LED open diagnostics. */
    /* VLED threshold 1: 1 V, encoded at 36 counts/V (36 raw). */
    {OTP_PAGE_1, OTP_LED_OPEN_THR_1, 1U * 36U},
    /* Encoded per-channel Open-threshold selector maps. */
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_0_3, 0x50U},
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_4_7, 0x51U},
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_8_11, 0x51U},
    {OTP_PAGE_1, OTP_LED_OPEN_SEL_12_15, 0x01U},

    /* Page 1: LED short diagnostics. */
    /* VDIF threshold 1: 3 V, encoded at 36 counts/V (108 raw). */
    {OTP_PAGE_1, OTP_LED_SHORT_THR_1, 3U * 36U},
    /* Encoded per-channel Short-threshold selector maps. */
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_0_3, 0x50U},
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_4_7, 0x51U},
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_8_11, 0x51U},
    {OTP_PAGE_1, OTP_LED_SHORT_SEL_12_15, 0x01U},

    /* Page 1: protection and communication. */
    /* ILED Open threshold: 2 mA, encoded at 10 counts/mA (20 raw). */
    {OTP_PAGE_1, OTP_ILED_OPEN_THR, 2U * 10U},
    {OTP_PAGE_1, OTP_VT_CRITICAL, 125U + 273U},    /* Critical temperature threshold: 125 degC. */
    {OTP_PAGE_1, OTP_COM_TIMEOUT, 0x00U},          /* Disable communication timeout. */

    /* Page 2: IDAC current range selection. */
    {OTP_PAGE_2, OTP_IDAC_REF_SEL_0_7, 0xFCU},     /* Select lower current range for CH2-CH7. */
    {OTP_PAGE_2, OTP_IDAC_REF_SEL_8_15, 0x03U},    /* Select lower current range for CH8-CH9. */

    /* Page 2: thermal derating. */
    {OTP_PAGE_2, OTP_VT_DERATE_START, 100U + 273U}, /* Start thermal derating at 100 degC. */
    {OTP_PAGE_2, OTP_VT_DERATE_STOP, 124U + 273U},  /* Stop derating progression at 124 degC. */
    {OTP_PAGE_2, OTP_DERATE_GAIN, (8U << 5U)},      /* VT gain: 3.125%/degC; VS gain: 0. */

    /* Page 2: diagnosis configuration. */
    /* Encoded DIAG output, operating-mode, and counter configuration. */
    {OTP_PAGE_2, OTP_DIAG_CONFIG, 0xC5U},
    {OTP_PAGE_2, OTP_DIAG0_CONFIG_0_7, 0xFCU},     /* Assign CH2-CH7 to diagnosis group 0. */
    {OTP_PAGE_2, OTP_DIAG0_CONFIG_8_15, 0x00U},    /* No CH8-CH15 channels assigned to DIAG0. */
    {OTP_PAGE_2, OTP_DIAG1_CONFIG_0_7, 0x00U},     /* No CH0-CH7 channels assigned to DIAG1. */
    {OTP_PAGE_2, OTP_DIAG1_CONFIG_8_15, 0x03U},    /* Assign CH8-CH9 to diagnosis group 1. */

    /* Page 2: current sink configuration. */
    /* {OTP_PAGE_2, OTP_ISINK_CONFIG, 0x00U}, */   /* Slew rate setting 0 (verify field mapping). */
    {OTP_PAGE_2, OTP_ISINK_BUNDLE, 0x10U},         /* Enable analog current bundling for CH8/CH9. */

    /* Page 2: LED supply selection. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_0_3, 0x00U},   /* Select VS as supply for CH0-CH3 diagnosis. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_4_7, 0x00U},   /* Select VS as supply for CH4-CH7 diagnosis. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_8_11, 0x00U},  /* Select VS as supply for CH8-CH11 diagnosis. */
    {OTP_PAGE_2, OTP_LED_SUPPLY_SEL_12_15, 0x00U}, /* Select VS as supply for CH12-CH15 diagnosis. */

    /* Page 3: LED channel enable configuration. */
    {OTP_PAGE_3, OTP_LED_ENABLE_0_7, 0xFCU},       /* Enable LED channels CH2-CH7. */
    {OTP_PAGE_3, OTP_LED_ENABLE_8_15, 0x03U},      /* Enable LED channels CH8-CH9. */

    /* Device identification and validation. */
    {OTP_PAGE_1, OTP_COM_DEV_ADDR, I2C_ADDR_VALID}, /* Configure normal-operation I2C address. */
    {OTP_PAGE_2, OTP_FOR_CUSTOMER_1, I2C_ADDR_VALID} /* Store customer validation value. */
};
#endif

#define LED_OTP_CONFIG_COUNT                                         \
    ((uint8)(sizeof(LedOtp_ConfigData) / sizeof(LedOtp_ConfigData[0])))

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/** \brief Starts the first page-select transaction of the OTP sequence. */
Std_ReturnType LedOtp_StartWrite(uint8 slaveAddress)
{
    Std_ReturnType result = STD_NOT_OK;

    LedOtp_ConfigIndex = (uint8)0U;
    LedOtp_WriteSlaveAddress = slaveAddress;
    LedOtp_WritePhase = LED_OTP_PHASE_IDLE;
    LedOtp_WriteActive = FALSE;
    LedOtp_TransferData = (uint16)0U;

    if (((slaveAddress == I2C_ADDR_INIT) ||
         (slaveAddress == I2C_ADDR_VALID)) &&
        (LED_OTP_CONFIG_COUNT > (uint8)0U) &&
        (LedOtp_IsConfigValid(&LedOtp_ConfigData[0]) == TRUE))
    {
        result = LedOtp_QueuePageSelect(LedOtp_WriteSlaveAddress,
                                        LedOtp_ConfigIndex);
        if (result == STD_OK)
        {
            LedOtp_WriteActive = TRUE;
            LedOtp_WritePhase = LED_OTP_PHASE_PAGE_SELECT;
        }
    }
    else
    {
        /* An empty or invalid table shall not start OTP programming. */
    }

    return result;
}

/** \brief Queues the next transaction after one OTP write completes. */
LedOtp_SequenceResultType LedOtp_ContinueWrite(void)
{
    LedOtp_SequenceResultType result = LED_OTP_SEQUENCE_ERROR;
    Std_ReturnType queueResult;

    if (LedOtp_WriteActive == TRUE)
    {
        switch (LedOtp_WritePhase)
        {
            case LED_OTP_PHASE_PAGE_SELECT:
                queueResult = LedOtp_QueueDataWrite();
                if (queueResult == STD_OK)
                {
                    LedOtp_WritePhase = LED_OTP_PHASE_DATA_WRITE;
                    result = LED_OTP_SEQUENCE_PENDING;
                }
                else
                {
                    LedOtp_AbortWrite();
                }
                break;

            case LED_OTP_PHASE_DATA_WRITE:
                LedOtp_ConfigIndex++;
                if (LedOtp_ConfigIndex >= LED_OTP_CONFIG_COUNT)
                {
                    LedOtp_AbortWrite();
                    result = LED_OTP_SEQUENCE_COMPLETE;
                }
                else if (LedOtp_IsConfigValid(
                             &LedOtp_ConfigData[LedOtp_ConfigIndex]) == TRUE)
                {
                    queueResult = LedOtp_QueuePageSelect(
                        LedOtp_WriteSlaveAddress,
                        LedOtp_ConfigIndex);
                    if (queueResult == STD_OK)
                    {
                        LedOtp_WritePhase = LED_OTP_PHASE_PAGE_SELECT;
                        result = LED_OTP_SEQUENCE_PENDING;
                    }
                    else
                    {
                        LedOtp_AbortWrite();
                    }
                }
                else
                {
                    LedOtp_AbortWrite();
                }
                break;

            case LED_OTP_PHASE_IDLE:
            default:
                LedOtp_AbortWrite();
                break;
        }
    }
    else
    {
        /* A completion notification is only valid during an active sequence. */
    }

    return result;
}

/** \brief Restores the OTP sequencer to its inactive state. */
void LedOtp_AbortWrite(void)
{
    LedOtp_WriteActive = FALSE;
    LedOtp_WritePhase = LED_OTP_PHASE_IDLE;
}

/** \brief Starts validating the configured OTP table at one slave address. */
Std_ReturnType LedOtp_StartValidityCheck(uint8 slaveAddress)
{
    Std_ReturnType result = STD_NOT_OK;

    LedOtp_ValidationIndex = (uint8)0U;
    LedOtp_ValidationSlaveAddress = slaveAddress;
    LedOtp_ValidationPhase = LED_OTP_VALIDATION_PHASE_IDLE;
    LedOtp_ValidationActive = FALSE;
    LedOtp_ValidationResult.valid = FALSE;
    LedOtp_ValidationResult.configIndex = (uint8)0U;
    LedOtp_ValidationResult.expectedValue = (uint16)0U;
    LedOtp_ValidationResult.actualValue = (uint16)0U;

    if ((LED_OTP_CONFIG_COUNT > (uint8)0U) &&
        (LedOtp_IsConfigValid(&LedOtp_ConfigData[0]) == TRUE))
    {
        result = LedOtp_QueuePageSelect(slaveAddress,
                                        LedOtp_ValidationIndex);
        if (result == STD_OK)
        {
            LedOtp_ValidationActive = TRUE;
            LedOtp_ValidationPhase =
                LED_OTP_VALIDATION_PHASE_PAGE_SELECT;
        }
        else
        {
            /* The first validation transaction was not accepted. */
        }
    }
    else
    {
        /* An empty or invalid table cannot be validated. */
    }

    return result;
}

/** \brief Advances page selection, register reading, and value comparison. */
LedOtp_SequenceResultType LedOtp_ContinueValidityCheck(void)
{
    LedOtp_SequenceResultType result = LED_OTP_SEQUENCE_ERROR;

    if (LedOtp_ValidationActive == TRUE)
    {
        switch (LedOtp_ValidationPhase)
        {
            case LED_OTP_VALIDATION_PHASE_PAGE_SELECT:
                result = LedOtp_ContinueValidationPageSelect();
                break;

            case LED_OTP_VALIDATION_PHASE_DATA_READ:
                result = LedOtp_ContinueValidationDataRead();
                break;

            case LED_OTP_VALIDATION_PHASE_IDLE:
            default:
                LedOtp_AbortValidityCheck();
                break;
        }
    }
    else
    {
        /* A completed job is only valid during an active validation. */
    }

    return result;
}

/** \brief Stops OTP validation while preserving the latest comparison data. */
void LedOtp_AbortValidityCheck(void)
{
    LedOtp_ValidationActive = FALSE;
    LedOtp_ValidationPhase = LED_OTP_VALIDATION_PHASE_IDLE;
    LedOtp_ValidationResult.valid = FALSE;
}

/** \brief Returns the latest OTP validation result. */
const LedOtp_ValidationResultType * LedOtp_GetValidationResult(void)
{
    return &LedOtp_ValidationResult;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Queues a one-word write that selects the current OTP page. */
static Std_ReturnType LedOtp_QueuePageSelect(uint8 slaveAddress,
                                             uint8 configIndex)
{
    LedOtp_TransferData =
        (uint16)LedOtp_ConfigData[configIndex].page;

    return E52248_Comm_StartWrite(slaveAddress,
                                  REG_PAGE_BASE_ADDR,
                                  E_SIZE_TYPE_1,
                                  &LedOtp_TransferData);
}

/** \brief Queues one OTP register word from the current table entry. */
static Std_ReturnType LedOtp_QueueDataWrite(void)
{
    LedOtp_TransferData = LedOtp_ConfigData[LedOtp_ConfigIndex].data;

    return E52248_Comm_StartWrite(
        LedOtp_WriteSlaveAddress,
        LedOtp_ConfigData[LedOtp_ConfigIndex].address,
        E_SIZE_TYPE_1,
        &LedOtp_TransferData);
}

/** \brief Queues a one-word read of the active OTP configuration address. */
static Std_ReturnType LedOtp_QueueValidationRead(void)
{
    return E52248_Comm_StartRead(
        LedOtp_ValidationSlaveAddress,
        LedOtp_ConfigData[LedOtp_ValidationIndex].address,
        E_SIZE_TYPE_1);
}

/** \brief Queues the data read after a successful page-select write. */
static LedOtp_SequenceResultType LedOtp_ContinueValidationPageSelect(void)
{
    LedOtp_SequenceResultType result = LED_OTP_SEQUENCE_ERROR;

    if (LedOtp_QueueValidationRead() == STD_OK)
    {
        LedOtp_ValidationPhase = LED_OTP_VALIDATION_PHASE_DATA_READ;
        result = LED_OTP_SEQUENCE_PENDING;
    }
    else
    {
        LedOtp_AbortValidityCheck();
    }

    return result;
}

/** \brief Compares one read value and queues the next configured address. */
static LedOtp_SequenceResultType LedOtp_ContinueValidationDataRead(void)
{
    LedOtp_SequenceResultType result = LED_OTP_SEQUENCE_ERROR;
    const LedOtp_ConfigType *config;
    uint16 actualValue = (uint16)0U;

    config = &LedOtp_ConfigData[LedOtp_ValidationIndex];
    LedOtp_ValidationResult.configIndex = LedOtp_ValidationIndex;
    LedOtp_ValidationResult.expectedValue = config->data;

    if (E52248_Comm_CopyReadData(&actualValue, (uint8)1U) != STD_OK)
    {
        LedOtp_AbortValidityCheck();
    }
    else if (actualValue != config->data)
    {
        LedOtp_ValidationResult.actualValue = actualValue;
        LedOtp_AbortValidityCheck();
        result = LED_OTP_SEQUENCE_INVALID;
    }
    else
    {
        LedOtp_ValidationResult.actualValue = actualValue;
        LedOtp_ValidationIndex++;

        if (LedOtp_ValidationIndex >= LED_OTP_CONFIG_COUNT)
        {
            LedOtp_ValidationActive = FALSE;
            LedOtp_ValidationPhase = LED_OTP_VALIDATION_PHASE_IDLE;
            LedOtp_ValidationResult.valid = TRUE;
            result = LED_OTP_SEQUENCE_COMPLETE;
        }
        else if (LedOtp_IsConfigValid(
                     &LedOtp_ConfigData[LedOtp_ValidationIndex]) == TRUE)
        {
            if (LedOtp_QueuePageSelect(LedOtp_ValidationSlaveAddress,
                                       LedOtp_ValidationIndex) == STD_OK)
            {
                LedOtp_ValidationPhase =
                    LED_OTP_VALIDATION_PHASE_PAGE_SELECT;
                result = LED_OTP_SEQUENCE_PENDING;
            }
            else
            {
                LedOtp_AbortValidityCheck();
            }
        }
        else
        {
            LedOtp_AbortValidityCheck();
        }
    }

    return result;
}

/** \brief Validates the page and 10-bit data fields of one table entry. */
static boolean LedOtp_IsConfigValid(const LedOtp_ConfigType *config)
{
    boolean result = FALSE;

    if ((config != NULL_PTR) &&
        (config->page <= (uint8)OTP_PAGE_3) &&
        (config->data <= LED_OTP_DATA_MAX))
    {
        result = TRUE;
    }
    else
    {
        /* Invalid configuration data shall not be sent to the device. */
    }

    return result;
}
