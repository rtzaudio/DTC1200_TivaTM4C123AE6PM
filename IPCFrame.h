/*
 * IPC - Serial InterProcess Message Protocol
 *
 * Copyright (C) 2016-2021, RTZ Professional Audio, LLC.
 *
 * ALL RIGHTS RESERVED.
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
 *      * Frame length: Length in bytes (always 4 for ACK/NAK only)
 *
 *      * Type: frame type 1=ACK/2=NAK (always 0x11H or 0x12H)
 *
 *      * ACK/NAK Sequence: ACK/NAK frame sequence#
 *
 *      * CRC value: CRC-16 value calculated from offset 2 to 6
 */

#ifndef _IPCFRAME_H_
#define _IPCFRAME_H_

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

/* IPC Frame Control Block */

typedef struct _IPC_FCB {
    /* frame data */
    uint8_t     type;                       /* frame type bits       */
    uint8_t     seqnum;                     /* frame tx/rx seq#      */
    uint8_t     acknak;                     /* frame ACK/NAK seq#    */
    uint8_t     rsvd;                       /* keep on 32-bit align  */
} IPC_FCB;

/*** IPC FRAME FUNCTIONS ***************************************************/

void IPC_FrameInit(IPC_FCB* fcb);
int IPC_FrameRx(UART_Handle handle, IPC_FCB* fcb, void* txtbuf, uint16_t* txtlen);
int IPC_FrameTx(UART_Handle handle, IPC_FCB* fcb, void* txtbuf, uint16_t txtlen);

#endif /* _IPCFRAME_H_ */
