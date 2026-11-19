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
    \file   e52248_comm.c
    \brief  Implements asynchronous E52248 I2C communication.
            This module owns the communication FSM, protocol frame conversion,
            CRC calculation, notification handling, and completed-job results.
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
| 2026-10-20 000.03     DL      Added completed-read data copy interface
********************************************************************************/

/*******************************************************************************
    4. HEADER FILES
*******************************************************************************/
#include "e52248_comm.h"
#include "knob_led_drv.h"

/*******************************************************************************
    5. PRIVATE DEFINITIONS AND MACROS
*******************************************************************************/
/** Internal receive, transmit, and payload buffer capacities. */
#define E52248_RX_BUFFER_LENGTH     ((uint8)21U)
#define E52248_TX_BUFFER_LENGTH     ((uint8)23U)
#define E52248_DATA_BUFFER_LENGTH   ((uint8)16U)

/** I2C direction bits used during protocol CRC calculation. */
#define E52248_I2C_READ  (0x01U)
#define E52248_I2C_WRITE (0x00U)

/** E52248 protocol CRC initial values and transfer-size markers. */
#define E52248_CRC6_INIT (0x3FU)
#define E52248_CRC8_INIT (0xFFU)
#define E52248_CRC_M_1   (0x00U)
#define E52248_CRC_M_4   (0x40U)
#define E52248_CRC_M_16  (0xC0U)

#define I2C_READ  E52248_I2C_READ
#define I2C_WRITE E52248_I2C_WRITE
#define CRC6_INIT E52248_CRC6_INIT
#define CRC8_INIT E52248_CRC8_INIT
#define CRC_M_1   E52248_CRC_M_1
#define CRC_M_4   E52248_CRC_M_4
#define CRC_M_16  E52248_CRC_M_16

/*******************************************************************************
    6. PRIVATE TYPEDEFS AND STRUCTURES
*******************************************************************************/
/** \brief Identifies an internal E52248 communication FSM state. */
typedef enum
{
    E52248_STATE_IDLE = 0U,
    E52248_STATE_READ_CMD_WAIT,
    E52248_STATE_READ_DATA_WAIT,
    E52248_STATE_WRITE_WAIT,
    E52248_STATE_COMPLETE,
    E52248_STATE_ERROR_COM,
    E52248_STATE_ERROR_CRC,
    E52248_STATE_MAX
} E52248_StateType;

/** \brief Identifies an event accepted by the communication FSM. */
typedef enum
{
    E52248_EVENT_START_READ = 0U,
    E52248_EVENT_START_WRITE,
    E52248_EVENT_TX_DONE,
    E52248_EVENT_RX_OK,
    E52248_EVENT_RX_CRC_ERROR,
    E52248_EVENT_HW_ERROR,
    E52248_EVENT_MAX
} E52248_EventType;

/** \brief Identifies the result of an internal CRC validation operation. */
typedef enum
{
    E_CRC_OK = 0U,
    E_CRC_NG,
    E_CHK_SEQ
} E52248_CommResultType;

/** \brief Associates a transfer size with its protocol marker bits. */
typedef struct
{
    E52248_CommSizeType size_type;
    uint8 crc_m;
} E52248_CommCrcType;

/** \brief Owns the statically allocated E52248 communication buffers. */
typedef struct
{
    uint16  rxConvBuf[E52248_RX_BUFFER_LENGTH];
    uint8   rxRawBuf[E52248_RX_BUFFER_LENGTH];
    uint8   txBuf[E52248_TX_BUFFER_LENGTH];
    uint16  dataBuf[E52248_DATA_BUFFER_LENGTH];
    uint16  rgbBuf[E52248_DATA_BUFFER_LENGTH];
} E52248_DrvBufType;

/*******************************************************************************
    7. PRIVATE FUNCTION PROTOTYPES
*******************************************************************************/
/* FSM state-entry, no-action, guard, and transition-action operations. */
static void E52248_EntryIdle(void);
static void E52248_EntryReadCommandWait(void);
static void E52248_EntryReadDataWait(void);
static void E52248_EntryWriteWait(void);
static void E52248_EntryComplete(void);
static void E52248_EntryErrorCom(void);
static void E52248_EntryErrorCrc(void);
static void E52248_NoAction(void);
static uint8 E52248_GuardAlways(void);
static void E52248_ActionNone(void);
static uint8 E52248_IsStartAllowed(void);
static uint8 E52248_GetWordCount(E52248_CommSizeType sizeType);
static void E52248_SetDefault(void);

/* E52248 frame conversion, validation, and CRC support functions. */
static void Comm_ConvertFrameRx(void);
static void Comm_ConvertFrameTx(void);
static E52248_CommResultType Comm_CheckCrc(void);
static uint8 Convert_4WordsWithCrcRx(const uint8 *pSrc, uint16 *pDes, uint8 crc);
static uint8 Convert_4WordsWithCrcTx(const uint16 *pSrc, uint8 *pDes, uint8 crc);
static inline uint8 Crc6_2C(uint8 data, uint8 crc);
static inline uint8 Crc8_97(uint8 data, uint8 crc);
static inline uint8 Crc8_A6(uint8 data, uint8 crc);

/*******************************************************************************
    8. PRIVATE VARIABLES
*******************************************************************************/
/** Runtime context owned by the E52248 communication module. */
static E52248_ContextType E52248_CommCtx;

/** Static storage for protocol and application payload buffers. */
static E52248_DrvBufType E52248_DrvBuffer;

/*******************************************************************************
    9. PRIVATE CONSTANTS AND FSM CONFIGURATION
*******************************************************************************/
/** Entry, exit, and periodic operations indexed by E52248_StateType. */
static const Fsm_StateOperationType E52248_CommStateOperations[E52248_STATE_MAX] =
{
    { E52248_EntryIdle,            E52248_NoAction, E52248_NoAction },
    { E52248_EntryReadCommandWait, E52248_NoAction, E52248_NoAction },
    { E52248_EntryReadDataWait,    E52248_NoAction, E52248_NoAction },
    { E52248_EntryWriteWait,       E52248_NoAction, E52248_NoAction },
    { E52248_EntryComplete,        E52248_NoAction, E52248_NoAction },
    { E52248_EntryErrorCom,        E52248_NoAction, E52248_NoAction },
    { E52248_EntryErrorCrc,        E52248_NoAction, E52248_NoAction }
};

/** Generates read and write start transitions for a reusable source state. */
#define E52248_START_TRANSITIONS(state_) \
    { (Fsm_StateType)(state_), (Fsm_EventType)E52248_EVENT_START_READ, \
      E52248_GuardAlways, E52248_ActionNone, \
      (Fsm_StateType)E52248_STATE_READ_CMD_WAIT }, \
    { (Fsm_StateType)(state_), (Fsm_EventType)E52248_EVENT_START_WRITE, \
      E52248_GuardAlways, E52248_ActionNone, \
      (Fsm_StateType)E52248_STATE_WRITE_WAIT }

/** State transition table evaluated for each dispatched communication event. */
static const Fsm_TransitionType E52248_CommTransitions[] =
{
    E52248_START_TRANSITIONS(E52248_STATE_IDLE),
    E52248_START_TRANSITIONS(E52248_STATE_COMPLETE),
    E52248_START_TRANSITIONS(E52248_STATE_ERROR_COM),
    E52248_START_TRANSITIONS(E52248_STATE_ERROR_CRC),
    { (Fsm_StateType)E52248_STATE_READ_CMD_WAIT,
      (Fsm_EventType)E52248_EVENT_TX_DONE,
      E52248_GuardAlways, E52248_ActionNone,
      (Fsm_StateType)E52248_STATE_READ_DATA_WAIT },
    { (Fsm_StateType)E52248_STATE_READ_CMD_WAIT,
      (Fsm_EventType)E52248_EVENT_HW_ERROR,
      E52248_GuardAlways, E52248_ActionNone,
      (Fsm_StateType)E52248_STATE_ERROR_COM },
    { (Fsm_StateType)E52248_STATE_READ_DATA_WAIT,
      (Fsm_EventType)E52248_EVENT_RX_OK,
      E52248_GuardAlways, E52248_ActionNone,
      (Fsm_StateType)E52248_STATE_COMPLETE },
    { (Fsm_StateType)E52248_STATE_READ_DATA_WAIT,
      (Fsm_EventType)E52248_EVENT_RX_CRC_ERROR,
      E52248_GuardAlways, E52248_ActionNone,
      (Fsm_StateType)E52248_STATE_ERROR_CRC },
    { (Fsm_StateType)E52248_STATE_READ_DATA_WAIT,
      (Fsm_EventType)E52248_EVENT_HW_ERROR,
      E52248_GuardAlways, E52248_ActionNone,
      (Fsm_StateType)E52248_STATE_ERROR_COM },
    { (Fsm_StateType)E52248_STATE_WRITE_WAIT,
      (Fsm_EventType)E52248_EVENT_TX_DONE,
      E52248_GuardAlways, E52248_ActionNone,
      (Fsm_StateType)E52248_STATE_COMPLETE },
    { (Fsm_StateType)E52248_STATE_WRITE_WAIT,
      (Fsm_EventType)E52248_EVENT_HW_ERROR,
      E52248_GuardAlways, E52248_ActionNone,
      (Fsm_StateType)E52248_STATE_ERROR_COM }
};

#define E52248_COMM_TRANSITION_COUNT \
    ((uint16)(sizeof(E52248_CommTransitions) / sizeof(E52248_CommTransitions[0])))

/** Generic FSM configuration used by the E52248 communication state machine. */
static const Fsm_ConfigType E52248_CommConfig =
{
    (Fsm_StateType)E52248_STATE_IDLE,
    (Fsm_StateType)E52248_STATE_MAX,
    (Fsm_EventType)E52248_EVENT_MAX,
    E52248_CommStateOperations,
    E52248_CommTransitions,
    E52248_COMM_TRANSITION_COUNT
};

/** Transfer-size marker table used during protocol frame conversion. */
static const E52248_CommCrcType com_crc_type[3] =
{
	{E_SIZE_TYPE_1, CRC_M_1},
	{E_SIZE_TYPE_4, CRC_M_4},
	{E_SIZE_TYPE_16, CRC_M_16}
};

/*******************************************************************************
    10. PUBLIC FUNCTIONS
*******************************************************************************/
/**
 * \brief Initializes the communication context, buffers, and internal FSM.
 *
 * \return STD_OK when initialization succeeds; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_Init(void)
{
    E52248_SetDefault();

#if (I2C_SCB3_ENABLE == STD_ON)
    E52248_CommCtx.i2c_ch = E52248_I2C_CHANNEL_SCB3;
#else
    E52248_CommCtx.i2c_ch = E52248_I2C_CHANNEL_SCB7;
#endif

    E52248_CommCtx.rx_buf = E52248_DrvBuffer.rxConvBuf;
    E52248_CommCtx.rx_raw_buf = E52248_DrvBuffer.rxRawBuf;

    E52248_CommCtx.tx_buf = E52248_DrvBuffer.txBuf;
    E52248_CommCtx.data_buf = E52248_DrvBuffer.dataBuf;
    E52248_CommCtx.rgb_buf = E52248_DrvBuffer.rgbBuf;

    return Fsm_Init(&E52248_CommCtx.fsm, &E52248_CommConfig);
}

/** \brief Processes hardware notifications and advances one FSM event. */
void E52248_Comm_MainFunction5ms(void)
{
    E52248_StateType state =
        (E52248_StateType)Fsm_GetState(&E52248_CommCtx.fsm);

    if (E52248_CommCtx.i2c_notiErr == STD_ON)
    {
        E52248_CommCtx.i2c_notiErr = STD_OFF;
        E52248_CommCtx.i2c_notiTx = STD_OFF;
        E52248_CommCtx.i2c_notiRx = STD_OFF;
        (void)Fsm_PostEvent(&E52248_CommCtx.fsm,
                            (Fsm_EventType)E52248_EVENT_HW_ERROR);
    }
    else if ((E52248_CommCtx.i2c_notiTx == STD_ON) &&
             ((state == E52248_STATE_READ_CMD_WAIT) ||
              (state == E52248_STATE_WRITE_WAIT)))
    {
        E52248_CommCtx.i2c_notiTx = STD_OFF;
        (void)Fsm_PostEvent(&E52248_CommCtx.fsm,
                            (Fsm_EventType)E52248_EVENT_TX_DONE);
    }
    else if ((E52248_CommCtx.i2c_notiRx == STD_ON) &&
             (state == E52248_STATE_READ_DATA_WAIT))
    {
        E52248_CommCtx.i2c_notiRx = STD_OFF;

        if (Comm_CheckCrc() == E_CRC_OK)
        {
            (void)Fsm_PostEvent(&E52248_CommCtx.fsm,
                                (Fsm_EventType)E52248_EVENT_RX_OK);
        }
        else
        {
            (void)Fsm_PostEvent(&E52248_CommCtx.fsm,
                                (Fsm_EventType)E52248_EVENT_RX_CRC_ERROR);
        }
    }
    else
    {
        /* No notification is ready for the active communication state. */
    }

    Fsm_Dispatch(&E52248_CommCtx.fsm);
    Fsm_Do(&E52248_CommCtx.fsm);
}

/**
 * \brief Queues an asynchronous E52248 register-read request.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_StartRead(uint8 slaveAddress,
                                     uint8 registerAddress,
                                     E52248_CommSizeType sizeType)
{
    Std_ReturnType result = STD_NOT_OK;

    if ((E52248_IsStartAllowed() == STD_TRUE) &&
        (sizeType < E_SIZE_TYPE_MAX))
    {
        E52248_CommCtx.i2c_addr = slaveAddress;
        E52248_CommCtx.reg_addr = registerAddress;
        E52248_CommCtx.size_type = sizeType;
        E52248_CommCtx.dir = E_READ;
        E52248_CommCtx.i2c_notiRx = STD_OFF;
        E52248_CommCtx.i2c_notiTx = STD_OFF;
        E52248_CommCtx.i2c_notiErr = STD_OFF;

        result = Fsm_PostEvent(&E52248_CommCtx.fsm,
                               (Fsm_EventType)E52248_EVENT_START_READ);
        if (result == STD_OK)
        {
            E52248_CommCtx.comm_sts = E_COMM_STS_BUSY;
            E52248_CommCtx.job_result = E52248_JOB_RESULT_PENDING;
        }
    }

    return result;
}

/**
 * \brief Copies the payload and queues an asynchronous register-write request.
 *
 * \return STD_OK when accepted; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_StartWrite(uint8 slaveAddress,
                                      uint8 registerAddress,
                                      E52248_CommSizeType sizeType,
                                      const uint16 *data)
{
    Std_ReturnType result = STD_NOT_OK;
    uint8 index;
    uint8 wordCount = E52248_GetWordCount(sizeType);

    if ((E52248_IsStartAllowed() == STD_TRUE) &&
        (data != NULL_PTR) &&
        (wordCount > (uint8)0U))
    {
        for (index = (uint8)0U; index < wordCount; index++)
        {
            E52248_CommCtx.data_buf[index] = data[index];
        }

        E52248_CommCtx.i2c_addr = slaveAddress;
        E52248_CommCtx.reg_addr = registerAddress;
        E52248_CommCtx.size_type = sizeType;
        E52248_CommCtx.dir = E_WRITE;
        E52248_CommCtx.i2c_notiRx = STD_OFF;
        E52248_CommCtx.i2c_notiTx = STD_OFF;
        E52248_CommCtx.i2c_notiErr = STD_OFF;

        result = Fsm_PostEvent(&E52248_CommCtx.fsm,
                               (Fsm_EventType)E52248_EVENT_START_WRITE);
        if (result == STD_OK)
        {
            E52248_CommCtx.comm_sts = E_COMM_STS_BUSY;
            E52248_CommCtx.job_result = E52248_JOB_RESULT_PENDING;
        }
    }

    return result;
}

/**
 * \brief Retrieves and consumes the latest completed communication result.
 *
 * \return STD_OK when a completed result is returned; otherwise STD_NOT_OK.
 */
Std_ReturnType E52248_Comm_TakeResult(
    E52248_JobResultType * const result)
{
    Std_ReturnType ret = STD_NOT_OK;

    if (result != NULL_PTR)
    {
        *result = E52248_JOB_RESULT_NONE;

        if ((E52248_CommCtx.job_result != E52248_JOB_RESULT_NONE) &&
            (E52248_CommCtx.job_result != E52248_JOB_RESULT_PENDING))
        {
            *result = E52248_CommCtx.job_result;
            E52248_CommCtx.job_result = E52248_JOB_RESULT_NONE;
            ret = STD_OK;
        }
        else
        {
            /* The active job has not completed yet. */
        }
    }
    else
    {
        /* Reject a missing result destination. */
    }

    return ret;
}

/** \brief Copies converted data from the latest completed read transaction. */
Std_ReturnType E52248_Comm_CopyReadData(uint16 * const data,
                                        uint8 wordCount)
{
    Std_ReturnType result = STD_NOT_OK;
    uint8 index;
    uint8 availableWordCount =
        E52248_GetWordCount(E52248_CommCtx.size_type);

    if ((data != NULL_PTR) &&
        (E52248_CommCtx.dir == E_READ) &&
        (E52248_CommCtx.comm_sts == E_COMM_STS_IDLE) &&
        (wordCount == availableWordCount) &&
        (wordCount > (uint8)0U))
    {
        for (index = (uint8)0U; index < wordCount; index++)
        {
            data[index] = E52248_CommCtx.rx_buf[index];
        }
        result = STD_OK;
    }
    else
    {
        /* Reject invalid destinations and mismatched or incomplete reads. */
    }

    return result;
}

/** \brief Records an asynchronous I2C receive-complete notification. */
void E52248_Comm_RxNotification(void)
{
    E52248_CommCtx.i2c_notiRx = STD_ON;
}

/** \brief Records an asynchronous I2C transmit-complete notification. */
void E52248_Comm_TxNotification(void)
{
    E52248_CommCtx.i2c_notiTx = STD_ON;
}

/** \brief Records an asynchronous I2C communication-error notification. */
void E52248_Comm_ErrorNotification(void)
{
    E52248_CommCtx.i2c_notiErr = STD_ON;
}

/** \brief Returns a read-only diagnostic view of the communication context. */
const E52248_ContextType * E52248_Comm_GetContext(void)
{
    return &E52248_CommCtx;
}

/*******************************************************************************
    11. PRIVATE FUNCTIONS
*******************************************************************************/
/** \brief Restores all communication context members to safe defaults. */
static void E52248_SetDefault(void)
{
    E52248_CommCtx.i2c_notiRx   = STD_OFF;
    E52248_CommCtx.i2c_notiTx   = STD_OFF;
    E52248_CommCtx.i2c_notiErr  = STD_OFF;
    E52248_CommCtx.i2c_addr     = I2C_ADDR_VALID;
    E52248_CommCtx.reg_addr     = (uint8)0U;

    E52248_CommCtx.dir          = E_WRITE;
    E52248_CommCtx.comm_sts     = E_COMM_STS_IDLE;
    E52248_CommCtx.size_type    = E_SIZE_TYPE_1;
    E52248_CommCtx.job_result   = E52248_JOB_RESULT_NONE;

    E52248_CommCtx.light_changed = STD_OFF;
    E52248_CommCtx.retry_cnt    = (uint8)0U;
    E52248_CommCtx.retry_otp_prog_cnt = (uint8)0U;

    E52248_CommCtx.rx_size      = (uint8)0U;
    E52248_CommCtx.tx_size      = (uint8)0U;
}

/** \brief Marks the communication service as idle. */
static void E52248_EntryIdle(void)
{
    E52248_CommCtx.comm_sts = E_COMM_STS_IDLE;
}

/** \brief Builds and transmits the command phase of an E52248 read job. */
static void E52248_EntryReadCommandWait(void)
{
    Comm_ConvertFrameRx();
}

/** \brief Starts the data-receive phase of an E52248 read job. */
static void E52248_EntryReadDataWait(void)
{
    I2C_E52248_ReadData(E52248_CommCtx.i2c_addr,
                        E52248_CommCtx.rx_raw_buf,
                        E52248_CommCtx.rx_size);
}

/** \brief Builds and transmits an E52248 write frame. */
static void E52248_EntryWriteWait(void)
{
    Comm_ConvertFrameTx();
}

/** \brief Latches the successful result of the active communication job. */
static void E52248_EntryComplete(void)
{
    if (E52248_CommCtx.dir == E_READ)
    {
        E52248_CommCtx.job_result = E52248_JOB_RESULT_READ_OK;
    }
    else
    {
        E52248_CommCtx.job_result = E52248_JOB_RESULT_WRITE_OK;
    }

    E52248_CommCtx.comm_sts = E_COMM_STS_IDLE;
}

/** \brief Latches an E52248 hardware communication error result. */
static void E52248_EntryErrorCom(void)
{
    E52248_CommCtx.comm_sts = E_COMM_STS_ERR_COM;
    E52248_CommCtx.job_result = E52248_JOB_RESULT_ERROR_COM;
}

/** \brief Latches an E52248 receive CRC error result. */
static void E52248_EntryErrorCrc(void)
{
    E52248_CommCtx.comm_sts = E_COMM_STS_ERR_CRC;
    E52248_CommCtx.job_result = E52248_JOB_RESULT_ERROR_CRC;
}

/** \brief Provides a reusable FSM operation with no behavior. */
static void E52248_NoAction(void)
{
    /* This state has no common exit or cyclic behavior. */
}

/**
 * \brief Unconditionally permits a configured communication transition.
 *
 * \return STD_TRUE for every invocation.
 */
static uint8 E52248_GuardAlways(void)
{
    return STD_TRUE;
}

/** \brief Provides a reusable transition action with no behavior. */
static void E52248_ActionNone(void)
{
    /* State entry functions start the required hardware operation. */
}

/**
 * \brief Checks whether a new asynchronous communication job may start.
 *
 * \return STD_TRUE when initialized and idle; otherwise STD_FALSE.
 */
static uint8 E52248_IsStartAllowed(void)
{
    uint8 result = STD_FALSE;

    if ((E52248_CommCtx.fsm.initialized == STD_TRUE) &&
        (E52248_CommCtx.comm_sts != E_COMM_STS_BUSY))
    {
        result = STD_TRUE;
    }

    return result;
}

/**
 * \brief Converts a transfer-size selector to its payload word count.
 *
 * \return 1, 4, or 16 for a valid selector; otherwise 0.
 */
static uint8 E52248_GetWordCount(E52248_CommSizeType sizeType)
{
    uint8 wordCount = (uint8)0U;

    if (sizeType == E_SIZE_TYPE_1)
    {
        wordCount = (uint8)1U;
    }
    else if (sizeType == E_SIZE_TYPE_4)
    {
        wordCount = (uint8)4U;
    }
    else if (sizeType == E_SIZE_TYPE_16)
    {
        wordCount = (uint8)16U;
    }
    else
    {
        /* Invalid size types map to zero words. */
    }

    return wordCount;
}

/** \brief Converts the active write request into an E52248 protocol frame. */
static void Comm_ConvertFrameTx(void)
{
    uint8 tmp_byte, crc;
    uint8 *pDes;
    uint16 const *pSrc;
    uint8 for_read;

    E52248_CommCtx.tx_size = 0u;
    E52248_CommCtx.dir = E_WRITE;
    E52248_CommCtx.comm_sts = E_COMM_STS_BUSY;

    tmp_byte = (uint8)(
        ((uint16)E52248_CommCtx.i2c_addr << 1U) |
        (uint16)I2C_WRITE);
    crc = Crc6_2C(tmp_byte, CRC6_INIT);

    pDes = E52248_CommCtx.tx_buf;
    pSrc = E52248_CommCtx.data_buf;

    pDes[0] = E52248_CommCtx.reg_addr;
    crc = Crc6_2C(E52248_CommCtx.reg_addr, crc);
    tmp_byte = com_crc_type[E52248_CommCtx.size_type].crc_m;

    if(E52248_CommCtx.size_type==E_SIZE_TYPE_1)
    {
        crc = Crc6_2C(tmp_byte, crc); /* M=1 */
        pDes[1] = crc;
        tmp_byte = (uint8)(pSrc[0] >> 2u);
        crc = Crc6_2C(tmp_byte, CRC6_INIT);
        pDes[2] = tmp_byte;
        tmp_byte = (uint8)(pSrc[0] << 6u);
        crc = Crc6_2C(tmp_byte, crc);
        tmp_byte |= crc;
        pDes[3] = tmp_byte;
        E52248_CommCtx.tx_size = 4u;
    }
    else if(E52248_CommCtx.size_type==E_SIZE_TYPE_4)
    {
        tmp_byte |= Crc6_2C(tmp_byte, crc); /* M=4 */
        pDes[1] = tmp_byte;
        tmp_byte = (uint8)(pSrc[0] >> 2u);
        crc = Crc8_97(tmp_byte, CRC8_INIT);
        pDes[2] = tmp_byte;
        tmp_byte = (uint8)(pSrc[0] << 6u);
        tmp_byte |= (uint8)(pSrc[1] >> 4u);
        crc = Crc8_97(tmp_byte, crc);
        pDes[3] = tmp_byte;
        tmp_byte = (uint8)(pSrc[1] << 4u);
        tmp_byte |= (uint8)(pSrc[2] >> 6u);
        crc = Crc8_97(tmp_byte, crc);
        pDes[4] = tmp_byte;
        tmp_byte = (uint8)(pSrc[2] << 2u);
        tmp_byte |= (uint8)(pSrc[3] >> 8u);
        crc = Crc8_97(tmp_byte, crc);
        pDes[5] = tmp_byte;
        tmp_byte = (uint8)pSrc[3];
        crc = Crc8_97(tmp_byte, crc);
        pDes[6] = tmp_byte;
        pDes[7] = crc;
        E52248_CommCtx.tx_size = 8u;
    }
    else if(E52248_CommCtx.size_type==E_SIZE_TYPE_16)
    {
        tmp_byte |= Crc6_2C(tmp_byte, crc); /* M=4 */
        pDes[1] = tmp_byte;
        crc = Convert_4WordsWithCrcTx(&pSrc[0], &pDes[2], CRC8_INIT);
        crc = Convert_4WordsWithCrcTx(&pSrc[4], &pDes[7], crc);
        crc = Convert_4WordsWithCrcTx(&pSrc[8], &pDes[12], crc);
        crc = Convert_4WordsWithCrcTx(&pSrc[12], &pDes[17], crc);
        pDes[22] = crc;
        E52248_CommCtx.tx_size = 23u;
    }
    else {
        E52248_CommCtx.tx_size = 0u;
    }

    /***********************************/
    /* for_read setting */
    /***********************************/
    /* A write transaction does not set an I2C read-direction mask. */
    for_read = (uint8)0U;

    if(E52248_CommCtx.tx_size != 0u) {
        I2C_E52248_WriteData(E52248_CommCtx.i2c_addr, pDes, E52248_CommCtx.tx_size, for_read);
    }
}

/** \brief Converts the active read request into an E52248 command frame. */
static void Comm_ConvertFrameRx(void)
{
    uint8 _tmp_byte, crc;
    uint8 for_read;

    E52248_CommCtx.dir = E_READ;
    E52248_CommCtx.comm_sts = E_COMM_STS_BUSY;

    E52248_CommCtx.tx_size = 2u;
    _tmp_byte = (uint8)(
        ((uint16)E52248_CommCtx.i2c_addr << 1U) |
        (uint16)I2C_WRITE);
    crc = Crc6_2C(_tmp_byte, CRC6_INIT);
    E52248_CommCtx.tx_buf[0] = E52248_CommCtx.reg_addr;
    crc = Crc6_2C(E52248_CommCtx.reg_addr, crc);

    if(E52248_CommCtx.size_type==E_SIZE_TYPE_1)
    {
        crc = Crc6_2C(0U, crc); /* M=1 */
        E52248_CommCtx.tx_buf[1] = crc;
        E52248_CommCtx.rx_size = 2u;
    }
    else if(E52248_CommCtx.size_type==E_SIZE_TYPE_4)
    {
        _tmp_byte = 0x40U; /* M=4 */
        _tmp_byte |= Crc6_2C(0x40U, crc);
        E52248_CommCtx.tx_buf[1] = _tmp_byte;
        E52248_CommCtx.rx_size = 6u;
    }
    else if(E52248_CommCtx.size_type==E_SIZE_TYPE_16)
    {
        _tmp_byte = (uint8)((uint16)0x03U << 6U); /* M=16 */
        _tmp_byte |= Crc6_2C(_tmp_byte, crc);
        E52248_CommCtx.tx_buf[1] = _tmp_byte;
        E52248_CommCtx.rx_size = 21u;
    }
    else {
        E52248_CommCtx.tx_size = 0u;
    }

    /***********************************/
    /* for_read setting */
    /***********************************/
    if (E52248_CommCtx.i2c_ch == E52248_I2C_CHANNEL_SCB3)
    {
        for_read = LEDDRV_SCB3_I2C_MASK;
    }
    else
    {
        for_read = LEDDRV_SCB7_I2C_MASK;
    }

    if(E52248_CommCtx.tx_size != 0u) {
        I2C_E52248_WriteData(E52248_CommCtx.i2c_addr, E52248_CommCtx.tx_buf, 
                             E52248_CommCtx.tx_size, for_read);
    }
}

/**
 * \brief Validates and converts the received E52248 protocol frame.
 *
 * \return E_CRC_OK when valid; otherwise E_CRC_NG.
 */
static E52248_CommResultType Comm_CheckCrc(void)
{
    E52248_CommResultType result = E_CRC_OK;
    uint8 crc;
    uint8 const *rx_raw_buf;
    uint16 *rx_buf;

    rx_raw_buf = E52248_CommCtx.rx_raw_buf;
    rx_buf = E52248_CommCtx.rx_buf;

    if(E52248_CommCtx.size_type == E_SIZE_TYPE_1)
    {
        crc = Crc6_2C(rx_raw_buf[0], CRC6_INIT);
        rx_buf[0] = (uint16)(
            ((uint16)rx_raw_buf[0] << 2U) |
            ((uint16)rx_raw_buf[1] >> 6U));
        crc = Crc6_2C(rx_raw_buf[1] & 0xC0U, crc);
        rx_buf[1] = crc;   /* for check crc value */

        if(crc != (rx_raw_buf[1] & 0x3F)) {
            result = E_CRC_NG;
        }
    }
    else if(E52248_CommCtx.size_type == E_SIZE_TYPE_4)
    {
        crc = Crc8_97(rx_raw_buf[0], CRC8_INIT);
        rx_buf[0] = ((uint16)rx_raw_buf[0]) << 2U;
        crc = Crc8_97(rx_raw_buf[1], crc);
        rx_buf[0] |= rx_raw_buf[1] >> 6U;
        rx_buf[1] = (uint16)(rx_raw_buf[1] & 0x3FU) << 4U;
        crc = Crc8_97(rx_raw_buf[2], crc);
        rx_buf[1] |= rx_raw_buf[2] >> 4U;
        rx_buf[2] = (uint16)(rx_raw_buf[2] & 0x0FU) << 6U;
        crc = Crc8_97(rx_raw_buf[3], crc);
        rx_buf[2] |= rx_raw_buf[3] >> 2U;
        rx_buf[3] = (uint16)(rx_raw_buf[3] & 0x03U) << 8U;
        crc = Crc8_97(rx_raw_buf[4], crc);
        rx_buf[3] |= rx_raw_buf[4];
        rx_buf[4] = crc;   /* for check crc value */

        if(crc != rx_raw_buf[5]) {
            result = E_CRC_NG;
        }
    }
    else if(E52248_CommCtx.size_type == E_SIZE_TYPE_16)
    {
        crc = Convert_4WordsWithCrcRx(&rx_raw_buf[0], &rx_buf[0], CRC8_INIT);
        crc = Convert_4WordsWithCrcRx(&rx_raw_buf[5], &rx_buf[4], crc);
        crc = Convert_4WordsWithCrcRx(&rx_raw_buf[10], &rx_buf[8], crc);
        crc = Convert_4WordsWithCrcRx(&rx_raw_buf[15], &rx_buf[12], crc);
        rx_buf[16] = crc;   /* for check crc value */

        if(crc != rx_raw_buf[20]) {
            result = E_CRC_NG;
        }
    }
    else 
    {
        result = E_CRC_NG;
    }

    return result;
}

/**
 * \brief Converts one packed four-word receive group and updates CRC-8.
 *
 * \return Updated CRC-8 accumulator.
 */
static uint8 Convert_4WordsWithCrcRx(const uint8 *pSrc, uint16 *pDes, uint8 crc)
{
	crc = Crc8_A6(pSrc[0], crc);
	pDes[0] = ((uint16)pSrc[0]) << 2U;
	crc = Crc8_A6(pSrc[1], crc);
	pDes[0] |= pSrc[1] >> 6U;
	pDes[1] = (uint16)(pSrc[1] & 0x3FU) << 4U;
	crc = Crc8_A6(pSrc[2], crc);
	pDes[1] |= pSrc[2] >> 4U;
	pDes[2] = (uint16)(pSrc[2] & 0x0FU) << 6U;
	crc = Crc8_A6(pSrc[3], crc);
	pDes[2] |= pSrc[3] >> 2U;
	pDes[3] = (uint16)(pSrc[3] & 0x03U) << 8U;
	crc = Crc8_A6(pSrc[4], crc);
	pDes[3] |= pSrc[4];

	return crc;
}

/**
 * \brief Packs one four-word transmit group and updates CRC-8.
 *
 * \return Updated CRC-8 accumulator.
 */
static uint8 Convert_4WordsWithCrcTx(const uint16 *pSrc, uint8 *pDes, uint8 crc)
{
	uint8 tmp_byte;

	tmp_byte = (uint8)(pSrc[0] >> 2);
	crc = Crc8_A6(tmp_byte, crc);
	pDes[0] = tmp_byte;
	tmp_byte = (uint8)(pSrc[0] << 6);
	tmp_byte |= (uint8)(pSrc[1] >> 4);
	crc = Crc8_A6(tmp_byte, crc);
	pDes[1] = tmp_byte;
	tmp_byte = (uint8)(pSrc[1] << 4);
	tmp_byte |= (uint8)(pSrc[2] >> 6);
	crc = Crc8_A6(tmp_byte, crc);
	pDes[2] = tmp_byte;

	tmp_byte = (uint8)(pSrc[2] << 2);
	tmp_byte |= (uint8)(pSrc[3] >> 8);
	crc = Crc8_A6(tmp_byte, crc);
	pDes[3] = tmp_byte;

	tmp_byte = (uint8)pSrc[3];
	crc = Crc8_A6(tmp_byte, crc);
	pDes[4] = tmp_byte;

	return crc;
}

/**
 * \brief Updates the E52248 CRC-6 accumulator with one byte.
 *
 * \return Updated CRC-6 accumulator.
 */
static inline uint8 Crc6_2C(uint8 data, uint8 crc)
{
  static const uint8 crc6[] = {
    0x00,0x32,0x29,0x1B,0x1F,0x2D,0x36,0x04,0x3E,0x0C,0x17,0x25,0x21,0x13,0x08,0x3A,
    0x31,0x03,0x18,0x2A,0x2E,0x1C,0x07,0x35,0x0F,0x3D,0x26,0x14,0x10,0x22,0x39,0x0B,
    0x2F,0x1D,0x06,0x34,0x30,0x02,0x19,0x2B,0x11,0x23,0x38,0x0A,0x0E,0x3C,0x27,0x15,
    0x1E,0x2C,0x37,0x05,0x01,0x33,0x28,0x1A,0x20,0x12,0x09,0x3B,0x3F,0x0D,0x16,0x24,
    0x13,0x21,0x3A,0x08,0x0C,0x3E,0x25,0x17,0x2D,0x1F,0x04,0x36,0x32,0x00,0x1B,0x29,
    0x22,0x10,0x0B,0x39,0x3D,0x0F,0x14,0x26,0x1C,0x2E,0x35,0x07,0x03,0x31,0x2A,0x18,
    0x3C,0x0E,0x15,0x27,0x23,0x11,0x0A,0x38,0x02,0x30,0x2B,0x19,0x1D,0x2F,0x34,0x06,
    0x0D,0x3F,0x24,0x16,0x12,0x20,0x3B,0x09,0x33,0x01,0x1A,0x28,0x2C,0x1E,0x05,0x37,
    0x26,0x14,0x0F,0x3D,0x39,0x0B,0x10,0x22,0x18,0x2A,0x31,0x03,0x07,0x35,0x2E,0x1C,
    0x17,0x25,0x3E,0x0C,0x08,0x3A,0x21,0x13,0x29,0x1B,0x00,0x32,0x36,0x04,0x1F,0x2D,
    0x09,0x3B,0x20,0x12,0x16,0x24,0x3F,0x0D,0x37,0x05,0x1E,0x2C,0x28,0x1A,0x01,0x33,
    0x38,0x0A,0x11,0x23,0x27,0x15,0x0E,0x3C,0x06,0x34,0x2F,0x1D,0x19,0x2B,0x30,0x02,
    0x35,0x07,0x1C,0x2E,0x2A,0x18,0x03,0x31,0x0B,0x39,0x22,0x10,0x14,0x26,0x3D,0x0F,
    0x04,0x36,0x2D,0x1F,0x1B,0x29,0x32,0x00,0x3A,0x08,0x13,0x21,0x25,0x17,0x0C,0x3E,
    0x1A,0x28,0x33,0x01,0x05,0x37,0x2C,0x1E,0x24,0x16,0x0D,0x3F,0x3B,0x09,0x12,0x20,
    0x2B,0x19,0x02,0x30,0x34,0x06,0x1D,0x2F,0x15,0x27,0x3C,0x0E,0x0A,0x38,0x23,0x11
  };

  return crc6[data^crc];
}

/**
 * \brief Updates the E52248 CRC-8 accumulator using polynomial 0x97.
 *
 * \return Updated CRC-8 accumulator.
 */
static inline uint8 Crc8_97(uint8 data, uint8 crc)
{
  static const uint8 crc8_97[] = {
    0x00,0xC7,0x67,0xA0,0xCE,0x09,0xA9,0x6E,0x75,0xB2,0x12,0xD5,0xBB,0x7C,0xDC,0x1B,
    0xEA,0x2D,0x8D,0x4A,0x24,0xE3,0x43,0x84,0x9F,0x58,0xF8,0x3F,0x51,0x96,0x36,0xF1,
    0x3D,0xFA,0x5A,0x9D,0xF3,0x34,0x94,0x53,0x48,0x8F,0x2F,0xE8,0x86,0x41,0xE1,0x26,
    0xD7,0x10,0xB0,0x77,0x19,0xDE,0x7E,0xB9,0xA2,0x65,0xC5,0x02,0x6C,0xAB,0x0B,0xCC,
    0x7A,0xBD,0x1D,0xDA,0xB4,0x73,0xD3,0x14,0x0F,0xC8,0x68,0xAF,0xC1,0x06,0xA6,0x61,
    0x90,0x57,0xF7,0x30,0x5E,0x99,0x39,0xFE,0xE5,0x22,0x82,0x45,0x2B,0xEC,0x4C,0x8B,
    0x47,0x80,0x20,0xE7,0x89,0x4E,0xEE,0x29,0x32,0xF5,0x55,0x92,0xFC,0x3B,0x9B,0x5C,
    0xAD,0x6A,0xCA,0x0D,0x63,0xA4,0x04,0xC3,0xD8,0x1F,0xBF,0x78,0x16,0xD1,0x71,0xB6,
    0xF4,0x33,0x93,0x54,0x3A,0xFD,0x5D,0x9A,0x81,0x46,0xE6,0x21,0x4F,0x88,0x28,0xEF,
    0x1E,0xD9,0x79,0xBE,0xD0,0x17,0xB7,0x70,0x6B,0xAC,0x0C,0xCB,0xA5,0x62,0xC2,0x05,
    0xC9,0x0E,0xAE,0x69,0x07,0xC0,0x60,0xA7,0xBC,0x7B,0xDB,0x1C,0x72,0xB5,0x15,0xD2,
    0x23,0xE4,0x44,0x83,0xED,0x2A,0x8A,0x4D,0x56,0x91,0x31,0xF6,0x98,0x5F,0xFF,0x38,
    0x8E,0x49,0xE9,0x2E,0x40,0x87,0x27,0xE0,0xFB,0x3C,0x9C,0x5B,0x35,0xF2,0x52,0x95,
    0x64,0xA3,0x03,0xC4,0xAA,0x6D,0xCD,0x0A,0x11,0xD6,0x76,0xB1,0xDF,0x18,0xB8,0x7F,
    0xB3,0x74,0xD4,0x13,0x7D,0xBA,0x1A,0xDD,0xC6,0x01,0xA1,0x66,0x08,0xCF,0x6F,0xA8,
    0x59,0x9E,0x3E,0xF9,0x97,0x50,0xF0,0x37,0x2C,0xEB,0x4B,0x8C,0xE2,0x25,0x85,0x42
  };

  return crc8_97[data^crc];
}

/**
 * \brief Updates the E52248 CRC-8 accumulator using polynomial 0xA6.
 *
 * \return Updated CRC-8 accumulator.
 */
static inline uint8 Crc8_A6(uint8 data, uint8 crc)
{
  static const uint8 crc8_A6[] = {
    0x00,0x3E,0x7C,0x42,0xF8,0xC6,0x84,0xBA,0x95,0xAB,0xE9,0xD7,0x6D,0x53,0x11,0x2F,
    0x4F,0x71,0x33,0x0D,0xB7,0x89,0xCB,0xF5,0xDA,0xE4,0xA6,0x98,0x22,0x1C,0x5E,0x60,
    0x9E,0xA0,0xE2,0xDC,0x66,0x58,0x1A,0x24,0x0B,0x35,0x77,0x49,0xF3,0xCD,0x8F,0xB1,
    0xD1,0xEF,0xAD,0x93,0x29,0x17,0x55,0x6B,0x44,0x7A,0x38,0x06,0xBC,0x82,0xC0,0xFE,
    0x59,0x67,0x25,0x1B,0xA1,0x9F,0xDD,0xE3,0xCC,0xF2,0xB0,0x8E,0x34,0x0A,0x48,0x76,
    0x16,0x28,0x6A,0x54,0xEE,0xD0,0x92,0xAC,0x83,0xBD,0xFF,0xC1,0x7B,0x45,0x07,0x39,
    0xC7,0xF9,0xBB,0x85,0x3F,0x01,0x43,0x7D,0x52,0x6C,0x2E,0x10,0xAA,0x94,0xD6,0xE8,
    0x88,0xB6,0xF4,0xCA,0x70,0x4E,0x0C,0x32,0x1D,0x23,0x61,0x5F,0xE5,0xDB,0x99,0xA7,
    0xB2,0x8C,0xCE,0xF0,0x4A,0x74,0x36,0x08,0x27,0x19,0x5B,0x65,0xDF,0xE1,0xA3,0x9D,
    0xFD,0xC3,0x81,0xBF,0x05,0x3B,0x79,0x47,0x68,0x56,0x14,0x2A,0x90,0xAE,0xEC,0xD2,
    0x2C,0x12,0x50,0x6E,0xD4,0xEA,0xA8,0x96,0xB9,0x87,0xC5,0xFB,0x41,0x7F,0x3D,0x03,
    0x63,0x5D,0x1F,0x21,0x9B,0xA5,0xE7,0xD9,0xF6,0xC8,0x8A,0xB4,0x0E,0x30,0x72,0x4C,
    0xEB,0xD5,0x97,0xA9,0x13,0x2D,0x6F,0x51,0x7E,0x40,0x02,0x3C,0x86,0xB8,0xFA,0xC4,
    0xA4,0x9A,0xD8,0xE6,0x5C,0x62,0x20,0x1E,0x31,0x0F,0x4D,0x73,0xC9,0xF7,0xB5,0x8B,
    0x75,0x4B,0x09,0x37,0x8D,0xB3,0xF1,0xCF,0xE0,0xDE,0x9C,0xA2,0x18,0x26,0x64,0x5A,
    0x3A,0x04,0x46,0x78,0xC2,0xFC,0xBE,0x80,0xAF,0x91,0xD3,0xED,0x57,0x69,0x2B,0x15
  };

  return crc8_A6[data^crc];
}

