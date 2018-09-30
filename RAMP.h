/*
 * RAMP - Remote Asynchronous Message Protocol
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC. ALL RIGHTS RESERVED.
 *
 *              *** RAMP Protocol Message Frame Structure ***
 *
 *                    +------------------------------+   Byte
 *                +-- |    SOF PREAMBLE (MSB=0x89)   |    0
 *                |   +------------------------------+
 *                |   |    SOF PREAMBLE (LSB=0xFC)   |    1
 *     Preamble --+   +------------------------------+
 *                |   |      FRAME LENGTH (MSB)      |    2
 *                |   +------------------------------+
 *                +-- |      FRAME LENGTH (LSB)      |    3
 *                    +---+---+---+---+--------------+
 *                +-- | E | D | P | A |    TYPE      |    4
 *                |   +---+---+---+---+--------------+
 *                |   |           ADDRESS            |    5
 *       Header --+   +------------------------------+
 *                |   |          SEQUENCE#           |    6
 *                |   +------------------------------+
 *                +-- |       ACK/NAK SEQUENCE#      |    7
 *                    +------------------------------+
 *                +-- |       TEXT LENGTH (MSB)      |    8
 *                |   +------------------------------+
 *                |   |       TEXT LENGTH (LSB)      |    9
 *                |   +------------------------------+
 *         Text --+   |              .               |    .
 *                |   |              .               |    .
 *                |   |          TEXT DATA           |    .
 *                |   |              .               |    .
 *                +-- |              .               |    .
 *                    +------------------------------+
 *                +-- |          CRC (MSB)           |   10 + textlen
 *          CRC --+   +------------------------------+
 *                +-- |          CRC (LSB)           |   11 + textlen
 *                    +------------------------------+
 * 
 *    RAMP Frame Contents Description:
 * 
 *      * SOF: start of frame preamble 0x89FC identifier.
 * 
 *      * Frame length: Total length less preamble (includes CRC bytes)
 * 
 *      * Flags: E=ERROR, D=DATAGRAM, P=PRIORITY, A=ACK/NAK response required
 *
 *      * Type: 1 = ACK-only           2 = NAK-only           3 = msg-only
 *              4 = msg+piggyback-ACK  5 = msg+piggyback-NAK
 * 
 *      * Address: Specifies the remote slave node address (0-16)
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
 *                  *** RAMP ACK/NAK Frame Structure ***
 *
 *                    +------------------------------+   byte
 *                +-- |    SOF PREAMBLE (MSB=0x89)   |    0
 *                |   +------------------------------+
 *                |   |    SOF PREAMBLE (LSB=0xBA)   |    1
 *     Preamble --+   +------------------------------+
 *                |   |     FRAME LENGTH (MSB=0)     |    2
 *                |   +------------------------------+
 *                +-- |     FRAME LENGTH (LSB=5)     |    3
 *                    +---+---+---+---+--------------+
 *                +-- | 0 | 0 | 0 | 1 |     TYPE     |    4
 *                |   +---+---+---+---+--------------+
 *       Header --+   |           ADDRESS            |    5
 *                |   +------------------------------+
 *                +-- |      ACK/NAK SEQUENCE#       |    6
 *                    +------------------------------+
 *                +-- |          CRC (MSB)           |    7
 *          CRC --+   +------------------------------+
 *                +-- |          CRC (LSB)           |    8
 *                    +------------------------------+
 *
 *    ACK/NAK Frame Content Description:
 *
 *      * SOF: start of frame preamble 0x89FC identifier.
 * 
 *      * Frame length: Length in bytes (always 5 for ACK/NAK only)
 * 
 *      * Type: frame type 1=ACK/2=NAK (always 11H or 12H)
 *
 *      * Address: Specifies the device node address (0-255)
 *
 *      * ACK/NAK Sequence: ACK/NAK frame sequence#
 *
 *      * CRC value: CRC-16 value calculated from offset 2 to 6
 */

#ifndef __RAMP_H
#define __RAMP_H

/*** RAMP Constants and Defines ********************************************/

#define PREAMBLE_MSB			0x89		/* first byte of preamble SOF  */
#define PREAMBLE_LSB			0xBA		/* second byte of preamble SOF */

#define MAX_WINDOW              8       	/* maximum window size         */

#define PREAMBLE_OVERHEAD       4       	/* preamble overhead (SOF+LEN) */
#define HEADER_OVERHEAD         4       	/* frame header overhead       */
#define TEXT_OVERHEAD           2       	/* text length overhead        */
#define CRC_OVERHEAD            2       	/* 16-bit CRC overhead         */
#define FRAME_OVERHEAD          ( PREAMBLE_OVERHEAD + HEADER_OVERHEAD + TEXT_OVERHEAD + CRC_OVERHEAD )

#define CRC_SEED_BYTE		    0xAB

#define MIN_SEQ_NUM             1       	/* min/max frame sequence num   */
#define MAX_SEQ_NUM             ( 3 * MAX_WINDOW )
#define NULL_SEQ_NUM            ( (uint8_t)0 )

#define ACK_FRAME_LEN           5
#define MAX_TEXT_LEN            2048
#define MIN_FRAME_LEN           ( FRAME_OVERHEAD - PREAMBLE_OVERHEAD )
#define MAX_FRAME_LEN           ( MIN_FRAME_LEN + MAX_TEXT_LEN )

#define INC_SEQ_NUM(n)		    ( (uint8_t)((n >= MAX_SEQ_NUM) ? MIN_SEQ_NUM : n+1) )

/* Frame Type Flag Bits (upper nibble) */
#define F_ACKNAK        		0x10		/* frame is ACK/NAK only frame */
#define F_PRIORITY      		0x20    	/* high priority message frame */
#define F_DATAGRAM        		0x40		/* no ACK/NAK required         */
#define F_ERROR         		0x80		/* frame error flag bit        */

#define FRAME_FLAG_MASK    		0xF0		/* flag mask is upper 4 bits   */

/* Frame Type Code (lower nibble) */
#define TYPE_ACK_ONLY   		1			/* ACK message frame only      */
#define TYPE_NAK_ONLY   		2			/* NAK message frame only      */
#define TYPE_MSG_ONLY   		3			/* message only frame          */
#define TYPE_MSG_ACK    		4			/* piggyback message plus ACK  */
#define TYPE_MSG_NAK    		5			/* piggyback message plus NAK  */

#define FRAME_TYPE_MASK    		0x0F		/* type mask is lower 4 bits   */

#define MAKETYPE(f, t)			( (uint8_t)((f & 0xF0) | (t & 0x0F)) )

/* Error Code Constants */
#define ERR_SUCCESS             0
#define ERR_TIMEOUT             1           /* comm port timeout           */
#define ERR_SYNC                2           /* SOF frame sync error        */
#define ERR_SHORT_FRAME         3           /* short rx-frame error        */
#define ERR_RX_OVERFLOW         4           /* rx buffer overflow          */
#define ERR_SEQ_NUM             5           /* bad sequence number         */
#define ERR_FRAME_TYPE          6           /* invalid frame type          */
#define ERR_FRAME_LEN           7           /* bad rx-frame length         */
#define ERR_TEXT_LEN            8           /* bad rx-text length          */
#define ERR_ACKNAK_LEN          9           /* bad rx-text length          */
#define ERR_CRC                 10          /* rx-frame checksum bad       */

/*** RAMP Structure Definitions ********************************************/

/* RAMP Frame Control Block Structure */

typedef struct fcb_t {
    uint8_t     type;               /* frame type bits       */
    uint8_t     seqnum;             /* frame tx/rx seq#      */
    uint8_t     acknak;             /* frame ACK/NAK seq#    */
    uint8_t     address;            /* tx/rx node address    */
} RAMP_FCB;

/*** RAMP Function Prototypes **********************************************/

void RAMP_InitFcb(RAMP_FCB* fcb);

int RAMP_TxFrame(UART_Handle handle, RAMP_FCB* fcb, void* text, uint16_t textlen);
int RAMP_RxFrame(UART_Handle handle, RAMP_FCB* fcb, void* text, uint16_t textlen);

#endif /* __RAMP_H */

/* end-of-file */
