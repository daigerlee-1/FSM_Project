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
    \file   prj_typedefs.h
    \brief  Project-wide fixed-width types and standard utility definitions.
            Uses AUTOSAR platform types or portable C99 types by configuration.
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
********************************************************************************/
#ifndef PRJ_TYPEDEFS_H
#define PRJ_TYPEDEFS_H

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "prj_defs.h"

#include <stdint.h>

/*******************************************************************************
    5. PUBLIC TYPEDEFS
*******************************************************************************/
typedef int8_t  sint8;
typedef int16_t sint16;
typedef int32_t sint32;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint8 boolean;
typedef uint8 Std_ReturnType;

typedef sint8  s8;
typedef sint16 s16;
typedef sint32 s32;
typedef uint8  u8;
typedef uint16 u16;
typedef uint32 u32;

/*******************************************************************************
    6. PUBLIC DEFINITIONS AND MACROS
*******************************************************************************/
#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

#ifndef FALSE
#define FALSE ((boolean)0U)
#endif

#ifndef TRUE
#define TRUE ((boolean)1U)
#endif

#ifndef STD_OK
#define STD_OK     ((Std_ReturnType)0U)
#endif

#ifndef STD_NOT_OK
#define STD_NOT_OK ((Std_ReturnType)1U)
#endif

#ifndef STD_ON
#define STD_ON      (1U)
#endif

#ifndef STD_OFF
#define STD_OFF     (0U)
#endif

#ifndef STD_FALSE
#define STD_FALSE   ((uint8)0U)
#endif

#ifndef STD_TRUE
#define STD_TRUE    ((uint8)1U)
#endif

#endif /* PRJ_TYPEDEFS_H */
