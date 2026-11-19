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
    \file   e52248_register.h
    \brief  Declares the E52248 register-address definitions.
            Register macros are grouped by volatile bus area, status area,
            OTP page, and immediate-update access type.
********************************************************************************
|-------------------------------------------------------------------------------
|    3. A U T H O R   I D E N T I T Y
|-------------------------------------------------------------------------------
| Initials     Name                      Company
| --------     ---------------------     ---------------------------------------
| FA           felix ALWOX               Mobase electronics
| DL           lee juhwan(DaigerLee)     Mobaseelectronic software design team
|
|-------------------------------------------------------------------------------
|               R E V I S I O N   H I S T O R Y
|-------------------------------------------------------------------------------
| Date       Ver        Author  Description
| ---------  ---        ------  ------------------------------------------------
| 2019-05-10 000.01     FA      Creation
| 2026-10-15 000.02     DL      Standardized public header documentation
********************************************************************************/
#ifndef DRV_E52248_REG_H
#define DRV_E52248_REG_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
/* This register-definition header has no include dependencies. */

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** E52248 volatile bus-configuration register area. */
#define IMM_UPDATE_MASK     		0x40U

#define REG_BUS_PULSE_0         0x00U
#define REG_BUS_PULSE_1         0x01U
#define REG_BUS_PULSE_2         0x02U
#define REG_BUS_PULSE_3         0x03U
#define REG_BUS_PULSE_4         0x04U
#define REG_BUS_PULSE_5         0x05U
#define REG_BUS_PULSE_6         0x06U
#define REG_BUS_PULSE_7         0x07U
#define REG_BUS_PULSE_8         0x08U
#define REG_BUS_PULSE_9         0x09U
#define REG_BUS_PULSE_10        0x0AU
#define REG_BUS_PULSE_11        0x0BU
#define REG_BUS_PULSE_12        0x0CU
#define REG_BUS_PULSE_13        0x0DU
#define REG_BUS_PULSE_14        0x0EU
#define REG_BUS_PULSE_15        0x0FU
#define REG_BUS_CURRENT_0       0x10U
#define REG_BUS_CURRENT_1       0x11U
#define REG_BUS_CURRENT_2       0x12U
#define REG_BUS_CURRENT_3       0x13U
#define REG_BUS_CURRENT_4       0x14U
#define REG_BUS_CURRENT_5       0x15U
#define REG_BUS_CURRENT_6       0x16U
#define REG_BUS_CURRENT_7       0x17U
#define REG_BUS_CURRENT_8       0x18U
#define REG_BUS_CURRENT_9       0x19U
#define REG_BUS_CURRENT_10      0x1AU
#define REG_BUS_CURRENT_11      0x1BU
#define REG_BUS_CURRENT_12      0x1CU
#define REG_BUS_CURRENT_13      0x1DU
#define REG_BUS_CURRENT_14      0x1EU
#define REG_BUS_CURRENT_15      0x1FU
#define REG_LED_ENABLE_0_7      0x22U
#define REG_LED_ENABLE_8_15     0x23U
#define REG_ASSERT_DIAG         0x31U
#define REG_BUS_DERATE_GAIN     0x38U
#define REG_BUS_PULSE_ALL       0x3CU
#define REG_BUS_CURRENT_ALL     0x3DU
#define REG_CMD_RESET           0x3EU
#define REG_PAGE_BASE_ADDR      0x70U
#define REG_CMD_UPDATE          0x7FU

/** E52248 volatile bus-status register area. */
#define REG_RESULT_VLED_0       0x80U
#define REG_RESULT_VLED_1       0x81U
#define REG_RESULT_VLED_2       0x82U
#define REG_RESULT_VLED_3       0x83U
#define REG_RESULT_VLED_4       0x84U
#define REG_RESULT_VLED_5       0x85U
#define REG_RESULT_VLED_6       0x86U
#define REG_RESULT_VLED_7       0x87U
#define REG_RESULT_VLED_8       0x88U
#define REG_RESULT_VLED_9       0x89U
#define REG_RESULT_VLED_10      0x8AU
#define REG_RESULT_VLED_11      0x8BU
#define REG_RESULT_VLED_12      0x8CU
#define REG_RESULT_VLED_13      0x8DU
#define REG_RESULT_VLED_14      0x8EU
#define REG_RESULT_VLED_15      0x8FU
#define REG_RESULT_VDIF_0       0x90U
#define REG_RESULT_VDIF_1       0x91U
#define REG_RESULT_VDIF_2       0x92U
#define REG_RESULT_VDIF_3       0x93U
#define REG_RESULT_VDIF_4       0x94U
#define REG_RESULT_VDIF_5       0x95U
#define REG_RESULT_VDIF_6       0x96U
#define REG_RESULT_VDIF_7       0x97U
#define REG_RESULT_VDIF_8       0x98U
#define REG_RESULT_VDIF_9       0x99U
#define REG_RESULT_VDIF_10      0x9AU
#define REG_RESULT_VDIF_11      0x9BU
#define REG_RESULT_VDIF_12      0x9CU
#define REG_RESULT_VDIF_13      0x9DU
#define REG_RESULT_VDIF_14      0x9EU
#define REG_RESULT_VDIF_15      0x9FU
#define REG_RESULT_ILED_0       0xA0U
#define REG_RESULT_ILED_1       0xA1U
#define REG_RESULT_ILED_2       0xA2U
#define REG_RESULT_ILED_3       0xA3U
#define REG_RESULT_ILED_4       0xA4U
#define REG_RESULT_ILED_5       0xA5U
#define REG_RESULT_ILED_6       0xA6U
#define REG_RESULT_ILED_7       0xA7U
#define REG_RESULT_ILED_8       0xA8U
#define REG_RESULT_ILED_9       0xA9U
#define REG_RESULT_ILED_10      0xAAU
#define REG_RESULT_ILED_11      0xABU
#define REG_RESULT_ILED_12      0xACU
#define REG_RESULT_ILED_13      0xADU
#define REG_RESULT_ILED_14      0xAEU
#define REG_RESULT_ILED_15      0xAFU
#define REG_RESULT_VT           0xB0U
#define REG_RESULT_VSUP         0xB1U
#define REG_RESULT_VDD5         0xB2U
#define REG_LED_OPEN_0_7        0xB8U
#define REG_LED_OPEN_8_15       0xB9U
#define REG_LED_SHORT_0_7       0xBAU
#define REG_LED_SHORT_8_15      0xBBU
#define REG_EVENT_STATUS        0xBCU
#define REG_PWMIN_STATUS        0xBDU
#define REG_DIAG_STATUS         0xBEU
#define REG_PROG_STATUS         0xBFU

/** E52248 OTP standalone area, page 0. */
#define OTP_PAGE_0              0U
#define OTP_LPFB_PULSE_0        0xC0U
#define OTP_LPFB_PULSE_1        0xC1U
#define OTP_LPFB_PULSE_2        0xC2U
#define OTP_LPFB_PULSE_3        0xC3U
#define OTP_LPFB_PULSE_4        0xC4U
#define OTP_LPFB_PULSE_5        0xC5U
#define OTP_LPFB_PULSE_6        0xC6U
#define OTP_LPFB_PULSE_7        0xC7U
#define OTP_LPFB_PULSE_8        0xC8U
#define OTP_LPFB_PULSE_9        0xC9U
#define OTP_LPFB_PULSE_10       0xCAU
#define OTP_LPFB_PULSE_11       0xCBU
#define OTP_LPFB_PULSE_12       0xCCU
#define OTP_LPFB_PULSE_13       0xCDU
#define OTP_LPFB_PULSE_14       0xCEU
#define OTP_LPFB_PULSE_15       0xCFU
#define OTP_HPFB_PULSE_0        0xD0U
#define OTP_HPFB_PULSE_1        0xD1U
#define OTP_HPFB_PULSE_2        0xD2U
#define OTP_HPFB_PULSE_3        0xD3U
#define OTP_HPFB_PULSE_4        0xD4U
#define OTP_HPFB_PULSE_5        0xD5U
#define OTP_HPFB_PULSE_6        0xD6U
#define OTP_HPFB_PULSE_7        0xD7U
#define OTP_HPFB_PULSE_8        0xD8U
#define OTP_HPFB_PULSE_9        0xD9U
#define OTP_HPFB_PULSE_10       0xDAU
#define OTP_HPFB_PULSE_11       0xDBU
#define OTP_HPFB_PULSE_12       0xDCU
#define OTP_HPFB_PULSE_13       0xDDU
#define OTP_HPFB_PULSE_14       0xDEU
#define OTP_HPFB_PULSE_15       0xDFU
#define OTP_LPFB_CURRENT_0      0xE0U
#define OTP_LPFB_CURRENT_1      0xE1U
#define OTP_LPFB_CURRENT_2      0xE2U
#define OTP_LPFB_CURRENT_3      0xE3U
#define OTP_LPFB_CURRENT_4      0xE4U
#define OTP_LPFB_CURRENT_5      0xE5U
#define OTP_LPFB_CURRENT_6      0xE6U
#define OTP_LPFB_CURRENT_7      0xE7U
#define OTP_LPFB_CURRENT_8      0xE8U
#define OTP_LPFB_CURRENT_9      0xE9U
#define OTP_LPFB_CURRENT_10     0xEAU
#define OTP_LPFB_CURRENT_11     0xEBU
#define OTP_LPFB_CURRENT_12     0xECU
#define OTP_LPFB_CURRENT_13     0xEDU
#define OTP_LPFB_CURRENT_14     0xEEU
#define OTP_LPFB_CURRENT_15     0xEFU
#define OTP_HPFB_CURRENT_0      0xF0U
#define OTP_HPFB_CURRENT_1      0xF1U
#define OTP_HPFB_CURRENT_2      0xF2U
#define OTP_HPFB_CURRENT_3      0xF3U
#define OTP_HPFB_CURRENT_4      0xF4U
#define OTP_HPFB_CURRENT_5      0xF5U
#define OTP_HPFB_CURRENT_6      0xF6U
#define OTP_HPFB_CURRENT_7      0xF7U
#define OTP_HPFB_CURRENT_8      0xF8U
#define OTP_HPFB_CURRENT_9      0xF9U
#define OTP_HPFB_CURRENT_10     0xFAU
#define OTP_HPFB_CURRENT_11     0xFBU
#define OTP_HPFB_CURRENT_12     0xFCU
#define OTP_HPFB_CURRENT_13     0xFDU
#define OTP_HPFB_CURRENT_14     0xFEU
#define OTP_HPFB_CURRENT_15     0xFFU

/** E52248 OTP standalone area, page 1. */
#define OTP_PAGE_1              1U
#define OTP_BIN_GAIN_0          0xC0U
#define OTP_BIN_GAIN_1          0xC1U
#define OTP_BIN_GAIN_2          0xC2U
#define OTP_BIN_GAIN_3          0xC3U
#define OTP_BIN_GAIN_4          0xC4U
#define OTP_BIN_GAIN_5          0xC5U
#define OTP_BIN_GAIN_6          0xC6U
#define OTP_BIN_GAIN_7          0xC7U
#define OTP_BIN_GAIN_8          0xC8U
#define OTP_BIN_GAIN_9          0xC9U
#define OTP_BIN_GAIN_10         0xCAU
#define OTP_BIN_GAIN_11         0xCBU
#define OTP_BIN_GAIN_12         0xCCU
#define OTP_BIN_GAIN_13         0xCDU
#define OTP_BIN_GAIN_14         0xCEU
#define OTP_BIN_GAIN_15         0xCFU
#define OTP_BIN_CLASS_CONFIG    0xD0U
#define OTP_BIN_CLASS_ENABLE_0_7    0xD1U
#define OTP_BIN_CLASS_ENABLE_8_15   0xD2U
#define OTP_BIN_CLASS_LEVEL_0   0xD3U
#define OTP_BIN_CLASS_LEVEL_1   0xD4U
#define OTP_BIN_CLASS_LEVEL_2   0xD5U
#define OTP_BIN_CLASS_LEVEL_3   0xD6U
#define OTP_BIN_CLASS_GAIN_0    0xD7U
#define OTP_BIN_CLASS_GAIN_1    0xD8U
#define OTP_BIN_CLASS_GAIN_2    0xD9U
#define OTP_BIN_CLASS_GAIN_3    0xDAU
#define OTP_BIN_CLASS_GAIN_4    0xDBU
#define OTP_PWM_PRESCALER       0xDCU
#define OTP_PWM_PERIOD          0xDDU
#define OTP_PWM_CONFIG          0xDEU
#define OTP_PWM_COMBINE_PRI     0xDFU
#define OTP_PWM_COMBINE_SEC     0xE0U
#define OTP_PWMIN_TIMING        0xE1U
#define OTP_PWMIN_ENABLE_0_3    0xE2U
#define OTP_PWMIN_ENABLE_4_7    0xE3U
#define OTP_PWMIN_ENABLE_8_11   0xE4U
#define OTP_PWMIN_ENABLE_12_15  0xE5U
#define OTP_LED_OPEN_THR_1      0xE8U
#define OTP_LED_OPEN_THR_2      0xE9U
#define OTP_LED_OPEN_THR_3      0xEAU
#define OTP_LED_OPEN_SEL_0_3    0xEBU
#define OTP_LED_OPEN_SEL_4_7    0xECU
#define OTP_LED_OPEN_SEL_8_11   0xEDU
#define OTP_LED_OPEN_SEL_12_15  0xEEU
#define OTP_LED_SHORT_THR_1     0xEFU
#define OTP_LED_SHORT_THR_2     0xF0U
#define OTP_LED_SHORT_THR_3     0xF1U
#define OTP_LED_SHORT_SEL_0_3   0xF2U
#define OTP_LED_SHORT_SEL_4_7   0xF3U
#define OTP_LED_SHORT_SEL_8_11  0xF4U
#define OTP_LED_SHORT_SEL_12_15 0xF5U
#define OTP_ILED_OPEN_THR       0xF7U
#define OTP_VS_TOO_LOW          0xF8U
#define OTP_VS_CRITICAL         0xF9U
#define OTP_VT_CRITICAL         0xFAU
#define OTP_COM_DEV_ADDR        0xFBU
#define OTP_COM_TIMEOUT         0xFCU

/** E52248 OTP standalone area, page 2. */
#define OTP_PAGE_2              2U
#define OTP_BUST_PULSE_0        0xC0U
#define OTP_BUST_PULSE_1        0xC1U
#define OTP_BUST_PULSE_2        0xC2U
#define OTP_BUST_PULSE_3        0xC3U
#define OTP_BUST_PULSE_4        0xC4U
#define OTP_BUST_PULSE_5        0xC5U
#define OTP_BUST_PULSE_6        0xC6U
#define OTP_BUST_PULSE_7        0xC7U
#define OTP_BUST_PULSE_8        0xC8U
#define OTP_BUST_PULSE_9        0xC9U
#define OTP_BUST_PULSE_10       0xCAU
#define OTP_BUST_PULSE_11       0xCBU
#define OTP_BUST_PULSE_12       0xCCU
#define OTP_BUST_PULSE_13       0xCDU
#define OTP_BUST_PULSE_14       0xCEU
#define OTP_BUST_PULSE_15       0xCFU
#define OTP_BUST_CURRENT_0      0xD0U
#define OTP_BUST_CURRENT_1      0xD1U
#define OTP_BUST_CURRENT_2      0xD2U
#define OTP_BUST_CURRENT_3      0xD3U
#define OTP_BUST_CURRENT_4      0xD4U
#define OTP_BUST_CURRENT_5      0xD5U
#define OTP_BUST_CURRENT_6      0xD6U
#define OTP_BUST_CURRENT_7      0xD7U
#define OTP_BUST_CURRENT_8      0xD8U
#define OTP_BUST_CURRENT_9      0xD9U
#define OTP_BUST_CURRENT_10     0xDAU
#define OTP_BUST_CURRENT_11     0xDBU
#define OTP_BUST_CURRENT_12     0xDCU
#define OTP_BUST_CURRENT_13     0xDDU
#define OTP_BUST_CURRENT_14     0xDEU
#define OTP_BUST_CURRENT_15     0xDFU
#define OTP_IDAC_REF_SEL_0_7    0xE0U
#define OTP_IDAC_REF_SEL_8_15   0xE1U
#define OTP_ISINK_CONFIG        0xE2U
#define OTP_VS_DERATE_RANGE     0xE3U
#define OTP_VT_DERATE_START     0xE4U
#define OTP_VT_DERATE_STOP      0xE5U
#define OTP_DERATE_GAIN         0xE6U
#define OTP_IO_CONFIG           0xE7U
#define OTP_DIAG_CONFIG         0xE8U
#define OTP_DIAG0_CONFIG_0_7    0xE9U
#define OTP_DIAG0_CONFIG_8_15   0xEAU
#define OTP_DIAG1_CONFIG_0_7    0xEBU
#define OTP_DIAG1_CONFIG_8_15   0xECU
#define OTP_DIAG_CURRENT        0xEDU
#define OTP_ISINK_BUNDLE        0xEEU
#define OTP_LED_SUPPLY_SEL_0_3  0xF0U
#define OTP_LED_SUPPLY_SEL_4_7  0xF1U
#define OTP_LED_SUPPLY_SEL_8_11 0xF2U
#define OTP_LED_SUPPLY_SEL_12_15 0xF3U
#define OTP_DEVICE_INFO         0xFDU
#define OTP_FOR_CUSTOMER_0      0xFEU
#define OTP_FOR_CUSTOMER_1      0xFFU

/** E52248 OTP standalone area, page 3. */
#define OTP_PAGE_3              3U
#define OTP_BUS_PULSE_0         0xC0U
#define OTP_BUS_PULSE_1         0xC1U
#define OTP_BUS_PULSE_2         0xC2U
#define OTP_BUS_PULSE_3         0xC3U
#define OTP_BUS_PULSE_4         0xC4U
#define OTP_BUS_PULSE_5         0xC5U
#define OTP_BUS_PULSE_6         0xC6U
#define OTP_BUS_PULSE_7         0xC7U
#define OTP_BUS_PULSE_8         0xC8U
#define OTP_BUS_PULSE_9         0xC9U
#define OTP_BUS_PULSE_10        0xCAU
#define OTP_BUS_PULSE_11        0xCBU
#define OTP_BUS_PULSE_12        0xCCU
#define OTP_BUS_PULSE_13        0xCDU
#define OTP_BUS_PULSE_14        0xCEU
#define OTP_BUS_PULSE_15        0xCFU
#define OTP_BUS_CURRENT_0       0xD0U
#define OTP_BUS_CURRENT_1       0xD1U
#define OTP_BUS_CURRENT_2       0xD2U
#define OTP_BUS_CURRENT_3       0xD3U
#define OTP_BUS_CURRENT_4       0xD4U
#define OTP_BUS_CURRENT_5       0xD5U
#define OTP_BUS_CURRENT_6       0xD6U
#define OTP_BUS_CURRENT_7       0xD7U
#define OTP_BUS_CURRENT_8       0xD8U
#define OTP_BUS_CURRENT_9       0xD9U
#define OTP_BUS_CURRENT_10      0xDAU
#define OTP_BUS_CURRENT_11      0xDBU
#define OTP_BUS_CURRENT_12      0xDCU
#define OTP_BUS_CURRENT_13      0xDDU
#define OTP_BUS_CURRENT_14      0xDEU
#define OTP_BUS_CURRENT_15      0xDFU
#define OTP_LED_ENABLE_0_7      0xE2U
#define OTP_LED_ENABLE_8_15     0xE3U
#define OTP_BUS_DERATE_GAIN     0xE9U
#define OTP_PAGE_BASE_ADDR      0xF0U
#define OTP_BUS_PULSE_ALL       0xFCU
#define OTP_BUS_CURRENT_ALL     0xFDU

/* Reserved OTP command aliases are intentionally not defined. */
/* #define REG_CMD_RESET           0xFEU */
/* #define REG_CMD_UPDATE          0xFFU */

/** Immediate-update aliases for selected volatile bus registers. */
#define REG_IMM_ENABLE_0_7			(REG_LED_ENABLE_0_7|IMM_UPDATE_MASK)
#define REG_IMM_ENABLE_8_15			(REG_LED_ENABLE_8_15|IMM_UPDATE_MASK)
#define	REG_IMM_BUS_PULSE_0			(REG_BUS_PULSE_0|IMM_UPDATE_MASK)
#define REG_IMM_BUS_PULSE_ALL       (REG_BUS_PULSE_ALL | IMM_UPDATE_MASK)
#define	REG_IMM_BUS_CURRENT_0		(REG_BUS_CURRENT_0|IMM_UPDATE_MASK)

/**
 * \brief Defines the E52248 slave addresses used to detect OTP programming.
 *
 * Communication initially checks the configured address (0x21). A NACK at
 * that address is interpreted as an unprogrammed OTP device address, so the
 * device is subsequently accessed through its initial address (0x3F) for OTP
 * programming.
 */
/** E52248 initial slave address (0x3F) used before OTP programming. */
#define I2C_ADDR_INIT       (0x1FU | 0x20U)
/** E52248 configured slave address (0x21) used for normal communication. */
#define I2C_ADDR_VALID      (0x01U | 0x20U)

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No public data types are declared by this module. */

/*******************************************************************************
    7. PUBLIC VARIABLES
*******************************************************************************/
/* No public variables are exported by this module. */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/* No public functions are declared by this module. */

#endif /* DRV_E52248_REG_H */
