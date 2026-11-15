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
    \file   lcan_input_rte.h
    \brief  Declares the AUTOSAR RTE adapter for local-CAN input messages.
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
| 2026-11-12 000.01     DL      Separated LCAN input RTE signal acquisition
********************************************************************************/
#ifndef LCAN_INPUT_RTE_H
#define LCAN_INPUT_RTE_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
/* This interface has no public include dependencies. */

/*******************************************************************************
    5. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
void LcanInputRte_ReadHuCarInfo(void);
void LcanInputRte_NotifyHuCarInfoTimeout(void);
void LcanInputRte_ReadHuEUsm(void);
void LcanInputRte_ReadHuMoodInfo(void);
void LcanInputRte_NotifyHuMoodInfoTimeout(void);
void LcanInputRte_ReadHuSys(void);
void LcanInputRte_ReadHuState(void);
void LcanInputRte_NotifyHuStateTimeout(void);
void LcanInputRte_ReadHuSysInfo(void);
void LcanInputRte_NotifyHuSysInfoTimeout(void);

#endif /* LCAN_INPUT_RTE_H */
