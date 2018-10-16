/*
 * SIMP - Serial InterProcess Message Protocol
 *
 * Copyright (C) 2018, RTZ Professional Audio, LLC. ALL RIGHTS RESERVED.
 *
 *              *** SIMP Protocol Message Frame Structure ***
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
 *    SIMP Frame Contents Description:
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
 *                  *** SIMP ACK/NAK Frame Structure ***
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

#ifndef _SIMP_H_
#define _SIMP_H_

/*** SIMP Constants and Defines ********************************************/

#define SIMP_PREAMBLE_MSB       0x79		/* first byte of preamble SOF  */
#define SIMP_PREAMBLE_LSB		0xBA		/* second byte of preamble SOF */

#define SIMP_MAX_WINDOW         8       	/* maximum window size         */

#define SIMP_PREAMBLE_OVERHEAD  4       	/* preamble overhead (SOF+LEN) */
#define SIMP_HEADER_OVERHEAD    3       	/* frame header overhead       */
#define SIMP_TEXT_OVERHEAD      2       	/* text length overhead        */
#define SIMP_CRC_OVERHEAD       2       	/* 16-bit CRC overhead         */
#define SIMP_FRAME_OVERHEAD     ( SIMP_PREAMBLE_OVERHEAD + SIMP_HEADER_OVERHEAD + \
                                  SIMP_TEXT_OVERHEAD + SIMP_CRC_OVERHEAD )

#define SIMP_CRC_SEED_BYTE		0xAB

#define SIMP_MIN_SEQ            1       	/* min/max frame sequence num   */
#define SIMP_MAX_SEQ            ( 3 * SIMP_MAX_WINDOW )
#define SIMP_NULL_SEQ           ( (uint8_t)0 )

#define SIMP_ACK_FRAME_LEN      4
#define SIMP_MAX_TEXT_LEN       2048
#define SIMP_MIN_FRAME_LEN      ( SIMP_FRAME_OVERHEAD - SIMP_PREAMBLE_OVERHEAD )
#define SIMP_MAX_FRAME_LEN      ( SIMP_MIN_FRAME_LEN + SIMP_MAX_TEXT_LEN )

#define SIMP_INC_SEQ(n)		    ( (uint8_t)((n >= SIMP_MAX_SEQ) ? SIMP_MIN_SEQ : n+1) )

/* Frame Type Flag Bits (upper nibble) */
#define SIMP_F_ACKNAK      		0x10		/* frame is ACK/NAK only frame */
#define SIMP_F_PRIORITY    		0x20    	/* high priority message frame */
#define SIMP_F_DATAGRAM    		0x40		/* no ACK/NAK required         */
#define SIMP_F_ERROR      		0x80		/* frame error flag bit        */

#define SIMP_FLAG_MASK	        0xF0		/* flag mask is upper 4 bits   */

/* Frame Type Code (lower nibble) */
#define SIMP_ACK_ONLY           1			/* ACK message frame only      */
#define SIMP_NAK_ONLY           2			/* NAK message frame only      */
#define SIMP_MSG_ONLY           3			/* message only frame          */
#define SIMP_MSG_ACK    	    4			/* piggyback message plus ACK  */
#define SIMP_MSG_NAK    	    5			/* piggyback message plus NAK  */
#define SIMP_MSG_USER           6           /* user defined message packet */

#define SIMP_TYPE_MASK          0x0F		/* type mask is lower 4 bits   */

#define SIMP_MAKETYPE(f, t)     ( (uint8_t)((f & 0xF0) | (t & 0x0F)) )

/* Error Code Constants */
#define SIMP_ERR_SUCCESS        0
#define SIMP_ERR_TIMEOUT        1           /* comm port timeout     */
#define SIMP_ERR_SYNC           2           /* SOF frame sync error  */
#define SIMP_ERR_SHORT_FRAME    3           /* short rx-frame error  */
#define SIMP_ERR_RX_OVERFLOW    4           /* rx buffer overflow    */
#define SIMP_ERR_SEQ_NUM        5           /* bad sequence number   */
#define SIMP_ERR_FRAME_TYPE     6           /* invalid frame type    */
#define SIMP_ERR_FRAME_LEN      7           /* bad rx-frame length   */
#define SIMP_ERR_ACK_LEN        8           /* bad ACK/NAK frame len */
#define SIMP_ERR_TEXT_LEN       9           /* bad rx-text length    */
#define SIMP_ERR_ACKNAK_LEN     10          /* bad rx-text length    */
#define SIMP_ERR_CRC            11          /* rx-frame checksum bad */

/*** SIMP Message Data Structure *******************************************/

typedef struct _SIMP_MSG {
    uint16_t        type;                   /* msg type code   */
    uint16_t        opcode;                 /* request op-code */
    union {
        int32_t     I;                      /* integer 32- bit */
        uint32_t    U;                      /* unsigned 32-bit */
        float       F;                      /* float           */
    } param1;                               /* param1          */
    union {
        int32_t     I;                      /* integer 32- bit */
        uint32_t    U;                      /* unsigned 32-bit */
        float       F;                      /* float           */
    }  param2;                              /* param2          */
} SIMP_MSG;

/*** SIMP Frame Control Block Data *****************************************/

typedef struct _SIMP_FCB {
    Queue_Elem  elem;                       /* queue link list ptrs  */
    uint8_t     type;                       /* frame type bits       */
    uint8_t     seqnum;                     /* frame tx/rx seq#      */
    uint8_t     acknak;                     /* frame ACK/NAK seq#    */
    uint32_t    state;                      /* error state for debug */
    size_t      textlen;                    /* text message length   */
    SIMP_MSG*   textbuf;                    /* text message buf ptr  */
} SIMP_FCB;

/*** SIMP Service Context Data *********************************************/

typedef struct _SIMP_SERVICE {
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
    int                 txNumFreeMsgs;
    int                 txErrors;
    uint32_t            txCount;
    uint8_t             txNextSeq;          /* next tx sequence# */
    int                 rxNumFreeMsgs;
    int                 rxErrors;
    uint32_t            rxCount;
    uint8_t             rxExpectedSeq;      /* expected recv seq#   */
    uint8_t             rxLastSeq;          /* last seq# accepted   */
    /* Transmit FCB and message buffers */
    SIMP_FCB*           txFCB;
    SIMP_MSG*           txMsg;
    /* Receive FCB and message buffers */
    SIMP_FCB*           rxFCB;
    SIMP_MSG*           rxMsg;
} SIMP_SERVICE;

/*** SIMP Function Prototypes **********************************************/

int SIMP_TxFrame(UART_Handle handle, SIMP_FCB* fcb);
int SIMP_RxFrame(UART_Handle handle, SIMP_FCB* fcb);

uint8_t SIMP_GetTxSeqNum(void);

#endif /* _SIMP_H_ */

/* end-of-file */
