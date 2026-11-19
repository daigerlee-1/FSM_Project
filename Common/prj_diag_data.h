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
    \file   prj_diag_data.h
    \brief  Defines vehicle-specific immutable diagnostic data.
            JX2 software-identification, part-number, and master-key data are
            selected through the project configuration in prj_defs.h.
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
| 2026-11-09 000.01     DL      Creation
| 2026-11-09 000.02     DL      Ordered diagnostic definitions by data group
| 2026-11-10 000.03     DL      Added encoded hardware-version data
********************************************************************************/
#ifndef PRJ_DIAG_DATA_H
#define PRJ_DIAG_DATA_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_defs.h"
#include "prj_typedefs.h"

/*******************************************************************************
    5. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
#if (VEHICLE_JX2_PRJ == 1U)

/* FOTA software-version bytes used by Fota_User_Callouts.c. */
#define ECU_SW_VER_DATA_0       ((uint8)'2')
#define ECU_SW_VER_DATA_1       ((uint8)'6')
#define ECU_SW_VER_DATA_2       ((uint8)'C')
#define ECU_SW_VER_DATA_3       ((uint8)'1')

/** Two-digit manufacture year. */
#define SW_VER_YYMS_YEAR        "26"

/** Encoded manufacture month and release sequence. */
#define SW_VER_YYMS_MONTH       "C1"

/* Main S/W Version, Manufacture Year(yy)/Month(m)/Times(n) */
#define ECU_SW_VER_DATA_INFO    SW_VER_YYMS_YEAR SW_VER_YYMS_MONTH
#define OEM_SW_VER_DATA_INFO    SW_VER_YYMS_YEAR "." SW_VER_YYMS_MONTH
#define OEM_SW_VER_DATA_LEN     5U

/* Declaration of ECU Unit Local CAN DB Version Information for UDS */
#define ECU_CANDB_VER_INFO      "2609"

/** Four hexadecimal digits used for the LCAN hardware-version signals. */
#define ECU_HW_VER_DATA_INFO    "0A00"

/** Display-form hardware version used by diagnostic identification data. */
#define ECU_HW_VER_INFO         "0A.00"

#if defined(DIAG_SVC_DEF) && (DIAG_SVC_DEF == 1U)

/* Declaration of Vehicle Name Information */
#define VEHICLE_INFO            "JX2"

/* Declaration of ECU Unit Information */
#define ECU_UNIT_INFO           "CCP"
#define ECU_UNIT_INFO_LEN       3U

/* Declaration of Supplier Code Information */
/* Based on factory write specifications
 *  When produced at the India plant: TELE
 *  When produced at plants other than the India plant: S110 */
#define ECU_SUP_CODE_INFO       "S110"
#define ECU_SUP_CODE_INFO_LEN   4U

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* Modify By JM.SEO.2024.10.10-Change Version Type For ES98765-02 OTA2.0 Format */
/* Version format: ES98765-02 OTA2.0 Format */
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* Sub S/W Version, Unit Type(dd)->General(01) */
#define ECU_SW_VER_UNIT_INFO    "01"
/* Sub1 S/W Version, Deveopment Type(dd)->Release(00), Develop(01~) */
#define ECU_SW_VER_DEVEL_INFO   "00"
/* Internal Deveopment S/W Version */
#define ECU_SW_VER_INTER_INFO   "00"

#define ECU_SW_VER_INFO_LEN     10U
#define ECU_SW_VER_INFO_INT_LEN 13U
#define ECU_SW_VER_INFO             \
    ECU_SW_VER_DATA_INFO "."        \
    ECU_SW_VER_UNIT_INFO "."        \
    ECU_SW_VER_DEVEL_INFO "."       \
    ECU_SW_VER_INTER_INFO

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* Version format: YYMS.Vehicle.Platform.ECU.Area.Major.Minor.YYMMDD.HHMMSS */
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#define SW_VER_PLATFORM         "CW"    /* Field of ccIC Platform */
#define SW_VER_AREA_INFO        "ALL"   /* Field of Sails Region */
#define SW_VER_MAJOR            "001"   /* Field of Major(001->Development , 002->Product) */
#define SW_VER_MINOR            "001"   /* Field of Minor(001~) For System Software Version Information */

#define ECU_SW_VERSION_LEN      41U
#define ECU_SW_VERSION              \
    ECU_SW_VER_DATA_INFO "."        \
    VEHICLE_INFO "."                \
    SW_VER_PLATFORM "."             \
    ECU_UNIT_INFO "."               \
    SW_VER_AREA_INFO "."            \
    SW_VER_MAJOR "."                \
    SW_VER_MINOR "."                \
    BUILD_TIME_STR

/** Number of part-number records configured for the JX2 vehicle. */
#define MAX_PART_NUM_TABLE (2U)

/** Number of bytes in one fixed-width JX2 part-number record. */
#define PRJ_DIAG_PART_NUMBER_LENGTH (10U)

/** Number of bytes in the JX2 diagnostic master key. */
#if (CCP_MASTERKEY_ENABLE == 1U)
#define MAX_MASTER_KEY (8U)
#endif

/*******************************************************************************
    6. PUBLIC TYPEDEFS AND STRUCTURES
*******************************************************************************/
/* No public data types are declared by this header. */

/*******************************************************************************
    7. PUBLIC CONSTANT DATA
*******************************************************************************/
/** JX2 software-identification payload for the diagnostic service. */
static const uint8 RXSWIN_V[] =
{
    (uint8)0x07U, (uint8)0xA2U,
    (uint8)0x0BU,
    (uint8)'R', (uint8)'1', (uint8)'0', (uint8)' ',
    (uint8)'J', (uint8)'X', (uint8)'2', (uint8)'/',
    (uint8)'1', (uint8)'/', (uint8)'0',
    (uint8)0x0CU,
    (uint8)'R', (uint8)'1', (uint8)'1', (uint8)'6',
    (uint8)' ', (uint8)'J', (uint8)'X', (uint8)'2',
    (uint8)'/', (uint8)'1', (uint8)'/', (uint8)'0',
    (uint8)0x0CU,
    (uint8)'R', (uint8)'1', (uint8)'5', (uint8)'5',
    (uint8)' ', (uint8)'J', (uint8)'X', (uint8)'2',
    (uint8)'/', (uint8)'1', (uint8)'/', (uint8)'0',
    (uint8)0x0CU,
    (uint8)'R', (uint8)'1', (uint8)'5', (uint8)'6',
    (uint8)' ', (uint8)'J', (uint8)'X', (uint8)'2',
    (uint8)'/', (uint8)'1', (uint8)'/', (uint8)'0'
};

/**
 * \brief Fixed-width JX2 part-number records exposed by diagnostic services.
 *
 * Each record contains exactly PRJ_DIAG_PART_NUMBER_LENGTH bytes and is not a
 * null-terminated C string.
 */
static const uint8
    PartNoTable[MAX_PART_NUM_TABLE][PRJ_DIAG_PART_NUMBER_LENGTH] =
{
    {
        (uint8)'9', (uint8)'6', (uint8)'5', (uint8)'4', (uint8)'0',
        (uint8)'H', (uint8)'K', (uint8)'0', (uint8)'0', (uint8)'0'
    },
    {
        (uint8)'9', (uint8)'6', (uint8)'5', (uint8)'4', (uint8)'0',
        (uint8)'H', (uint8)'K', (uint8)'0', (uint8)'1', (uint8)'0'
    }
};

#if (CCP_MASTERKEY_ENABLE == 1U)
/**
 * \brief JX2 diagnostic master-key byte sequence.
 *
 * Internal linkage keeps the header-only definition link-safe when this file
 * is included by more than one translation unit.
 */
static const uint8 MasterKeyTable[MAX_MASTER_KEY] =
{
    (uint8)0x4AU, (uint8)0x58U, (uint8)0x32U, (uint8)0x08U,
    (uint8)0x4AU, (uint8)0x58U, (uint8)0x32U, (uint8)0x08U
};
#endif

#endif /* defined(DIAG_SVC_DEF) && (DIAG_SVC_DEF == 1U) */

#endif  /* VEHICLE_JX2_PRJ == 1U */

/*******************************************************************************
    8. PUBLIC FUNCTION PROTOTYPES
*******************************************************************************/
/* No public functions are declared by this header. */

#endif /* PRJ_DIAG_DATA_H */

