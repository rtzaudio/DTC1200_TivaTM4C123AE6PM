/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================
 *
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================ */

#ifndef _IPCTASK_H_
#define _IPCTASK_H_

#include "RAMP.h"
#include "IPCMessage.h"

/* ============================================================================
 * IPC Message Structure
 * ============================================================================ */

typedef struct _IPCMSG {
    uint16_t        type;           /* the IPC message type code   */
    uint16_t        opcode;         /* application defined op code */
    union {
        uint32_t    U;
        float       F;
    } param1;                       /* unsigned or float param1 */
    union {
        uint32_t    U;
        float       F;
    }  param2;                      /* unsigned or float param2 */
} IPCMSG;

/* ============================================================================
 * IPC Tx/Rx Message List Element Structure
 * ============================================================================ */

typedef struct _IPC_ELEM {
	Queue_Elem	elem;
	FCB			fcb;
    IPCMSG      msg;
} IPC_ELEM;

typedef struct _IPC_ACK {
    uint8_t     status;
    uint8_t     acknak;
    uint8_t     retry;
    uint8_t     type;
    IPCMSG      msg;
} IPC_ACK;

/* ============================================================================
 * IPC Message Server Object
 * ============================================================================ */

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
    Bool (*datagramHandlerFxn)(IPCMSG* msg, FCB* fcb);
    Bool (*transactionHandlerFxn)(IPCMSG* msg, FCB* fcb, UInt32 timeout);
    /* frame memory buffers */
    IPC_ELEM*           txBuf;
    IPC_ELEM*           rxBuf;
    IPC_ACK*            ackBuf;
} IPCSVR_OBJECT;

/* ============================================================================
 * IPC Function Prototypes
 * ============================================================================ */

Bool IPC_Server_init(void);

/* Application specific callback handlers */
Bool IPC_Handle_datagram(IPCMSG* msg, FCB* fcb);
Bool IPC_Handle_transaction(IPCMSG* msg, FCB* fcb, UInt32 timeout);

/* IPC server internal use */
Bool IPC_Message_post(IPCMSG* msg, FCB* fcb, UInt32 timeout);
Bool IPC_Message_pend(IPCMSG* msg, FCB* fcb, UInt32 timeout);
uint8_t IPC_GetTxSeqNum(void);

/* High level functions to send messages */
Bool IPC_Notify(IPCMSG* msg, UInt32 timeout);
Bool IPC_Transaction(IPCMSG* msg, UInt32 timeout);

#endif /* _IPCTASK_H_ */
