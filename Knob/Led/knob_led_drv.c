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
    \file   knob_led_drv.c
    \brief  Implements the E52248 hardware abstraction interface.
            AUTOSAR builds use RTE services, while standalone builds provide
            deterministic host-test behavior without target hardware.
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
| 2026-10-15 000.02     DL      Standardized C source documentation
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "knob_led_drv.h"

#if (PRJ_MOBILGENE_ENABLE == 1U)
#include "Rte_SWC_KnobLed.h"
#include "cdd_i2c.h"
#endif

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/* No private definitions or macros are declared by this module. */

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No private data types are declared by this module. */

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* No private functions are declared by this module. */

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/* No private variables are declared by this module. */

/*******************************************************************************
    9. PRIVATE CONSTANTS AND CONFIGURATION
*******************************************************************************/
/* No private constants or configuration tables are declared by this module. */

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
#if (PRJ_MOBILGENE_ENABLE == 1U)
/** \brief Reads the E52248 DIAG0 input through the AUTOSAR RTE. */
uint8 GpioGet_E52248_Diag0(void)
{
    static IoHwAb_LevelType ledDiag0Input;

    (void)Rte_Call_R_DigDir_LED_DIAG0_Input_ReadDirect(&ledDiag0Input);

    return (uint8)ledDiag0Input;
}

/** \brief Reads the E52248 DIAG1 input through the AUTOSAR RTE. */
uint8 GpioGet_E52248_Diag1(void)
{
    static IoHwAb_LevelType ledDiag1Input;

    (void)Rte_Call_R_DigDir_LED_DIAG1_Input_ReadDirect(&ledDiag1Input);

    return (uint8)ledDiag1Input;
}

/** \brief Writes the E52248 supply-enable output through the AUTOSAR RTE. */
void GpioSet_E52248_VccEnable(uint8 vLevel)
{
    (void)Rte_Call_R_DigDir_LED_VCC_EN_Output_WriteDirect((IoHwAb_LevelType)vLevel);
}

/** \brief Writes the E52248 output-enable signal through the AUTOSAR RTE. */
void GpioSet_E52248_OutputEnable(uint8 vLevel)
{
    (void)Rte_Call_R_DigDir_LED_OE_Output_WriteDirect((IoHwAb_LevelType)vLevel);
}

/** \brief Writes the E52248 OTP programming signal through the AUTOSAR RTE. */
void GpioSet_E52248_OtpProg(uint8 vLevel)
{
    (void)Rte_Call_R_DigDir_LED_PROG_Output_WriteDirect((IoHwAb_LevelType)vLevel);
}

/** \brief Writes the active-low E52248 reset signal through the AUTOSAR RTE. */
void GpioSet_E52248_nReset(uint8 vLevel)
{
    IoHwAb_LevelType physicalLevel;

    if (vLevel == STD_ON)
    {
        /* Logical reset ON drives the active-low RESET_N pin low. */
        physicalLevel = (IoHwAb_LevelType)STD_OFF;
    }
    else
    {
        /* Logical reset OFF releases the RESET_N pin high. */
        physicalLevel = (IoHwAb_LevelType)STD_ON;
    }

#if (I2C_SCB3_ENABLE == STD_ON)
    (void)Rte_Call_R_DigDir_LED_RST_N_Output_WriteDirect(physicalLevel);
#else
    (void)Rte_Call_R_DigDir_LED_RST_N_Output2_WriteDirect(physicalLevel);
#endif
}

/** \brief Initializes the target I2C channel through the AUTOSAR RTE. */
void I2C_E52248_Init(void)
{
#if (I2C_SCB3_ENABLE == STD_ON)
    (void)Rte_Call_R_I2C_CS_Init_Init(SCB3_I2C);
#else
    (void)Rte_Call_R_I2C_CS_Init_Init(SCB7_I2C);
#endif
}

/** \brief Starts an E52248 I2C read through the AUTOSAR RTE. */
void I2C_E52248_ReadData(uint8 slave_addr, uint8 *data, uint8 len)
{
#if (I2C_SCB3_ENABLE == STD_ON)
    (void)Rte_Call_R_I2C_CS_ReadData_ReadData(SCB3_I2C, data, len, slave_addr);
#else
    (void)Rte_Call_R_I2C_CS_ReadData_ReadData(SCB7_I2C, data, len, slave_addr);
#endif
}

/** \brief Starts an E52248 I2C write through the AUTOSAR RTE. */
void I2C_E52248_WriteData(uint8 slave_addr, const uint8 *data, uint8 len, uint8 for_read)
{
#if (I2C_SCB3_ENABLE == STD_ON)
    (void)Rte_Call_R_I2C_CS_WriteData_WriteData(SCB3_I2C, data, len, slave_addr, for_read);
#else
    (void)Rte_Call_R_I2C_CS_WriteData_WriteData(SCB7_I2C, data, len, slave_addr, for_read);
#endif
}
#else
/** \brief Returns the deterministic standalone DIAG0 input level. */
uint8 GpioGet_E52248_Diag0(void)
{
    return STD_OFF;
}

/** \brief Returns the deterministic standalone DIAG1 input level. */
uint8 GpioGet_E52248_Diag1(void)
{
    return STD_OFF;
}

/** \brief Accepts a supply-enable request in the standalone build. */
void GpioSet_E52248_VccEnable(uint8 vLevel)
{
    (void)vLevel;
}

/** \brief Accepts an output-enable request in the standalone build. */
void GpioSet_E52248_OutputEnable(uint8 vLevel)
{
    (void)vLevel;
}

/** \brief Accepts an OTP programming request in the standalone build. */
void GpioSet_E52248_OtpProg(uint8 vLevel)
{
    (void)vLevel;
}

/** \brief Accepts a reset request in the standalone build. */
void GpioSet_E52248_nReset(uint8 vLevel)
{
    (void)vLevel;
}

/** \brief Provides the standalone I2C initialization stub. */
void I2C_E52248_Init(void)
{
    /* No hardware initialization is required for the standalone PC build. */
}

/** \brief Provides deterministic zero-filled standalone receive data. */
void I2C_E52248_ReadData(uint8 slave_addr, uint8 *data, uint8 len)
{
    uint8 index;

    (void)slave_addr;

    if (data != NULL_PTR)
    {
        for (index = (uint8)0U; index < len; index++)
        {
            data[index] = (uint8)0U;
        }
    }
}

/** \brief Accepts an I2C write request in the standalone build. */
void I2C_E52248_WriteData(uint8 slave_addr, const uint8 *data, uint8 len, uint8 for_read)
{
    (void)slave_addr;
    (void)data;
    (void)len;
    (void)for_read;
}
#endif

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/* No private function implementations are defined by this module. */
