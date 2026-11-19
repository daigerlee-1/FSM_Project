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
    \file   knob_led_drv.h
    \brief  Declares the hardware abstraction interface used by the LED module.
             This interface provides E52248 GPIO control and asynchronous I2C
             access.
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
| 2026-10-14 000.02     DL      Standardized public header documentation
********************************************************************************/
#ifndef KNOB_LED_DRV_H
#define KNOB_LED_DRV_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
/** Bit mask selecting the SCB3 I2C read-request flag. */
#define LEDDRV_SCB3_I2C_MASK ((uint8)(1U << 0U))

/** Bit mask selecting the SCB7 I2C read-request flag. */
#define LEDDRV_SCB7_I2C_MASK ((uint8)(1U << 4U))

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
/**
 * \brief Reads the E52248 DIAG0 input level.
 *
 * \return STD_ON when DIAG0 is active; otherwise STD_OFF.
 */
uint8 GpioGet_E52248_Diag0(void);

/**
 * \brief Reads the E52248 DIAG1 input level.
 *
 * \return STD_ON when DIAG1 is active; otherwise STD_OFF.
 */
uint8 GpioGet_E52248_Diag1(void);

/**
 * \brief Controls the E52248 supply-enable output.
 *
 * \param[in] vLevel Requested digital output level.
 */
void GpioSet_E52248_VccEnable(uint8 vLevel);

/**
 * \brief Controls the E52248 LED output-enable signal.
 *
 * \param[in] vLevel Requested digital output level.
 */
void GpioSet_E52248_OutputEnable(uint8 vLevel);

/**
 * \brief Controls the E52248 OTP programming-enable signal.
 *
 * \param[in] vLevel Requested digital output level.
 */
void GpioSet_E52248_OtpProg(uint8 vLevel);

/**
 * \brief Controls the E52248 active-low reset signal.
 *
 * The driver converts the logical request to the physical active-low level.
 *
 * \param[in] vLevel STD_ON requests reset assertion; STD_OFF releases reset.
 */
void GpioSet_E52248_nReset(uint8 vLevel);

/** Initializes the I2C channel used by the E52248 device. */
void I2C_E52248_Init(void);

/**
 * \brief Starts an asynchronous I2C read from the E52248 device.
 *
 * Completion or failure is reported through the LED communication
 * notification interface.
 *
 * \param[in]  slave_addr 7-bit I2C slave address.
 * \param[out] data       Destination buffer for the received bytes.
 * \param[in]  len        Number of bytes to receive.
 */
void I2C_E52248_ReadData(uint8 slave_addr, uint8 *data, uint8 len);

/**
 * \brief Starts an asynchronous I2C write to the E52248 device.
 *
 * Completion or failure is reported through the LED communication
 * notification interface.
 *
 * \param[in] slave_addr 7-bit I2C slave address.
 * \param[in] data       Source buffer containing bytes to transmit.
 * \param[in] len        Number of bytes to transmit.
 * \param[in] for_read   Channel-specific read-sequence control bit field.
 */
void I2C_E52248_WriteData(uint8 slave_addr, const uint8 *data, uint8 len,
                          uint8 for_read);

#endif /* KNOB_LED_DRV_H */
