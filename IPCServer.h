/*
 * IPC - Serial InterProcess Message Protocol
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC. ALL RIGHTS RESERVED.
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 *              *** IPC Protocol Message Frame Structure ***
 *
 *                    +------------------------------+   Byte
 *                +-- |    SOF PREAMBLE (MSB=0x79)   |    0
 *                |   +------------------------------+
 *                |   |    SOF PREAMBLE (LSB=0xBA)   |    1
 *     Preamble --+   +------------------------------+
 *                |   |      FRAME LENGTH (MSB)      |    2
 *                |   +------------------------------+
 *                +-- |      FRAME LENGTH (LSB)      |    3
 *                    +---+---+---+---+--------------+
 *                +-- | E | D | P | A |    TYPE      |    4
 *                |   +---+---+---+---+--------------+
 *       Header --+   |          SEQUENCE#           |    5
 *                |   +------------------------------+
 *                +-- |       ACK/NAK SEQUENCE#      |    6
 *                    +------------------------------+
 *                +-- |       TEXT LENGTH (MSB)      |    7
 *                |   +------------------------------+
 *                |   |       TEXT LENGTH (LSB)      |    8
 *                |   +------------------------------+
 *         Text --+   |              .               |    .
 *                |   |              .               |    .
 *                |   |          TEXT DATA           |    .
 *                |   |              .               |    .
 *                +-- |              .               |    .
 *                    +------------------------------+
 *                +-- |          CRC (MSB)           |    9 + textlen
 *          CRC --+   +------------------------------+
 *                +-- |          CRC (LSB)           |   10 + textlen
 *                    +------------------------------+
 *
 *
 *    IPC Frame Contents Description:
 *
 *      * SOF: start of frame preamble 0x79BA identifier.
 *
 *      * Frame length: Total length less preamble (includes CRC bytes)
 *
 *      * Flags: E=ERROR, D=DATAGRAM, P=PRIORITY, A=ACK/NAK response required
 *
 *      * Type: 1 = ACK-only           2 = NAK-only           3 = msg-only
 *              4 = msg+piggyback-ACK  5 = msg+piggyback-NAK  6 = user defined
 *
 *      * Sequence#: Transmit frame sequence number (1-24)
 *
 *      * ACK/NAK Seq#: Receive piggyback ACK/NAK sequence# (0 if none)
 *
 *      * Text Length: length of text data segment (0-1024)
 *
 *      * Text Data: Segment text bytes (if text-length nonzero)
 *
 *      * CRC value: CRC-16 value calculated from offset 2 to end of text data
 *
 *
 *                  *** IPC ACK/NAK Frame Structure ***
 *
 *                    +------------------------------+   byte
 *                +-- |    SOF PREAMBLE (MSB=0x79)   |    0
 *                |   +------------------------------+
 *                |   |    SOF PREAMBLE (LSB=0xBA)   |    1
 *     Preamble --+   +------------------------------+
 *                |   |     FRAME LENGTH (MSB=0)     |    2
 *                |   +------------------------------+
 *                +-- |     FRAME LENGTH (LSB=4)     |    3
 *                    +---+---+---+---+--------------+
 *                +-- | 0 | 0 | 0 | 1 |     TYPE     |    4
 *       Header --+   +---+---+---+---+--------------+
 *                +-- |      ACK/NAK SEQUENCE#       |    5
 *                    +------------------------------+
 *                +-- |          CRC (MSB)           |    6
 *          CRC --+   +------------------------------+
 *                +-- |          CRC (LSB)           |    7
 *                    +------------------------------+
 *
 *    ACK/NAK Frame Content Description:
 *
 *      * SOF: start of frame preamble 0x79BA identifier.
 *
 *      * Frame length: Length in bytes (always 5 for ACK/NAK only)
 *
 *      * Type: frame type 1=ACK/2=NAK (always 11H or 12H)
 *
 *      * ACK/NAK Sequence: ACK/NAK frame sequence#
 *
 *      * CRC value: CRC-16 value calculated from offset 2 to 6
 */

#ifndef _IPCTASK_H_
#define _IPCTASK_H_

#include "CRC16.h"
#include "IPCMessage.h"

/*** IPC Constants and Defines *********************************************/

#define IPC_PREAMBLE_MSB        0x79        /* first byte of preamble SOF  */
#define IPC_PREAMBLE_LSB        0xBA        /* second byte of preamble SOF */

#define IPC_MAX_WINDOW          8           /* maximum window size         */

#define IPC_PREAMBLE_OVERHEAD   4           /* preamble overhead (SOF+LEN) */
#define IPC_HEADER_OVERHEAD     3           /* frame header overhead       */
#define IPC_TEXT_OVERHEAD       2           /* text length overhead        */
#define IPC_CRC_OVERHEAD        2           /* 16-bit CRC overhead         */
#define IPC_FRAME_OVERHEAD      ( IPC_PREAMBLE_OVERHEAD + IPC_HEADER_OVERHEAD + \
                                  IPC_TEXT_OVERHEAD + IPC_CRC_OVERHEAD )

#define IPC_CRC_SEED_BYTE       0xAB

#define IPC_MIN_SEQ             1           /* min/max frame sequence num   */
#define IPC_MAX_SEQ             ( 3 * IPC_MAX_WINDOW )
#define IPC_NULL_SEQ            ( (uint8_t)0 )

#define IPC_ACK_FRAME_LEN       4
#define IPC_MAX_TEXT_LEN        512
#define IPC_MIN_FRAME_LEN       ( IPC_FRAME_OVERHEAD - IPC_PREAMBLE_OVERHEAD )
#define IPC_MAX_FRAME_LEN       ( IPC_MIN_FRAME_LEN + IPC_MAX_TEXT_LEN )

#define IPC_INC_SEQ(n)          ( (uint8_t)((n >= IPC_MAX_SEQ) ? IPC_MIN_SEQ : n+1) )

/* Frame Type Flag Bits (upper nibble) */
#define IPC_F_ACKNAK            0x10        /* frame is ACK/NAK only frame */
#define IPC_F_PRIORITY          0x20        /* high priority message frame */
#define IPC_F_DATAGRAM          0x40        /* no ACK/NAK required         */
#define IPC_F_ERROR             0x80        /* frame error flag bit        */

#define IPC_FLAG_MASK           0xF0        /* flag mask is upper 4 bits   */

/* Frame Type Code (lower nibble) */
#define IPC_ACK_ONLY            1           /* ACK message frame only      */
#define IPC_NAK_ONLY            2           /* NAK message frame only      */
#define IPC_MSG_ONLY            3           /* message only frame          */
#define IPC_MSG_ACK             4           /* piggyback message plus ACK  */
#define IPC_MSG_NAK             5           /* piggyback message plus NAK  */
#define IPC_MSG_USER            6           /* user defined message packet */

#define IPC_TYPE_MASK           0x0F        /* type mask is lower 4 bits   */

#define IPC_MAKETYPE(f, t)      ( (uint8_t)((f & 0xF0) | (t & 0x0F)) )

/* Error Code Constants */
#define IPC_ERR_SUCCESS         0
#define IPC_ERR_TIMEOUT         1           /* comm port timeout     */
#define IPC_ERR_SYNC            2           /* SOF frame sync error  */
#define IPC_ERR_SHORT_FRAME     3           /* short rx-frame error  */
#define IPC_ERR_RX_OVERFLOW     4           /* rx buffer overflow    */
#define IPC_ERR_SEQ_NUM         5           /* bad sequence number   */
#define IPC_ERR_FRAME_TYPE      6           /* invalid frame type    */
#define IPC_ERR_FRAME_LEN       7           /* bad rx-frame length   */
#define IPC_ERR_ACK_LEN         8           /* bad ACK/NAK frame len */
#define IPC_ERR_TEXT_LEN        9           /* bad rx-text length    */
#define IPC_ERR_ACKNAK_LEN      10          /* bad rx-text length    */
#define IPC_ERR_CRC             11          /* rx-frame checksum bad */

/*** IPC MESSAGE STRUCTURE *************************************************/

typedef struct _IPC_MSG {
    uint16_t        type;           /* the IPC message type code   */
    uint16_t        opcode;         /* application defined op code */
    union {
        int32_t     I;
        uint32_t    U;
        float       F;
    } param1;                       /* unsigned or float param1 */
    union {
        int32_t     I;
        uint32_t    U;
        float       F;
    }  param2;                      /* unsigned or float param2 */
} IPC_MSG;

/*** IPC TX/RX MESSAGE LIST ELEMENT STRUCTURE ******************************/

typedef struct _IPC_FCB {
    uint8_t     type;               /* frame type bits       */
    uint8_t     seqnum;             /* frame tx/rx seq#      */
    uint8_t     acknak;             /* frame ACK/NAK seq#    */
    uint8_t     address;            /* tx/rx node address    */
} IPC_FCB;

typedef struct _IPC_ELEM {
	Queue_Elem  elem;
	IPC_FCB     fcb;
    IPC_MSG     msg;
} IPC_ELEM;

typedef struct _IPC_ACK {
    uint8_t     flags;
    uint8_t     acknak;
    uint8_t     retry;
    uint8_t     type;
    IPC_MSG     msg;
} IPC_ACK;

/*** IPC MESSAGE SERVER OBJECT *********************************************/

typedef struct _IPCSVR_OBJECT {
	UART_Handle         uartHandle;
    /* tx queues and semaphores */
	Queue_Handle        txFreeQue;
    Queue_Handle        txDataQue;
    Semaphore_Handle    txDataSem;
    Semaphore_Handle    txFreeSem;
    Event_Handle        ackEvent;
    /* rx queues and semaphores */
    Queue_Handle        rxFreeQue;
    Queue_Handle        rxDataQue;
    Semaphore_Handle    rxDataSem;
    Semaphore_Handle    rxFreeSem;
    /* server data items */
    int					txNumFreeMsgs;
    int                 txErrors;
    uint32_t			txCount;
    uint8_t             txNextSeq;          /* next tx sequence# */
    int                 rxNumFreeMsgs;
    int                 rxErrors;
    uint32_t            rxCount;
    uint8_t             rxExpectedSeq;		/* expected recv seq#   */
    uint8_t             rxLastSeq;       	/* last seq# accepted   */
    /* callback handlers */
    //Bool (*datagramHandlerFxn)(IPC_MSG* msg, IPC_FCB* fcb);
    //Bool (*transactionHandlerFxn)(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);
    /* frame memory buffers */
    IPC_ELEM*           txBuf;
    IPC_ELEM*           rxBuf;
    IPC_ACK*            ackBuf;
} IPCSVR_OBJECT;

/*** IPC FUNCTION PROTOTYPES ***********************************************/

Bool IPC_Server_init(void);
Bool IPC_Server_startup(void);

uint8_t IPC_GetTxSeqNum(void);

int IPC_TxFrame(UART_Handle handle, IPC_FCB* fcb, void* txtbuf, uint16_t txtlen);
int IPC_RxFrame(UART_Handle handle, IPC_FCB* fcb, void* txtbuf, uint16_t txtlen);

/* Application specific callback handlers */
Bool IPC_Handle_datagram(IPC_MSG* msg, IPC_FCB* fcb);
Bool IPC_Handle_transaction(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);

/* IPC server internal use */
Bool IPC_Message_post(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);
Bool IPC_Message_pend(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);

/* High level functions to send messages */
Bool IPC_Notify(IPC_MSG* msg, UInt32 timeout);
Bool IPC_Transaction(IPC_MSG* msgTx, IPC_MSG* msgRx, UInt32 timeout);

#endif /* _IPCTASK_H_ */
