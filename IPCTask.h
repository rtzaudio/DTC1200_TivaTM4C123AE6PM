/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016, RTZ Professional Audio, LLC
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

/* Message types for IPCMSG.type */
#define IPC_TYPE_NOTIFY				100
#define IPC_TYPE_TRANSACTION		101

/* Operation Codes for IPCMSG.opcode */
#define OP_NOTIFY_BUTTON			10
#define OP_NOTIFY_TRANSPORT			11

/* ============================================================================
 * IPC Message Structure
 * ============================================================================ */

typedef struct _IPCMSG {
    uint32_t    type;                    /* command code   */
    uint32_t    opcode;                  /* operation code */
    union {
        uint32_t    U;
        float       F;
    } param1;
    union {
        uint32_t    U;
        float       F;
    }  param2;
} IPCMSG;

/* ============================================================================
 * IPC Message List Entry Structure
 * ============================================================================ */

typedef struct _FCBMSG {
	Queue_Elem	elem;
	FCB			fcb;
    IPCMSG      msg;
} FCBMSG;

/* ============================================================================
 * IPC Message Server Object
 * ============================================================================ */

typedef struct _IPCSVR_OBJECT {
	UART_Handle         uartHandle;
	Queue_Handle        txFreeQue;
    Queue_Handle        txDataQue;
    Semaphore_Handle    txDataSem;
    Semaphore_Handle    txFreeSem;
    int					numFreeMsgs;
    uint32_t			txCount;
    uint32_t			rxCount;
    uint8_t             currSeq;            /* current tx sequence# */
    uint8_t             expectSeq;          /* expected recv seq#   */
    uint8_t             lastSeq;            /* last seq# accepted   */
    FCBMSG*             txMsgBuf;
    FCBMSG*             rxMsgBuf;
} IPCSVR_OBJECT;

/* Function Prototypes */

Bool IPC_Server_init(void);

Bool IPC_Send_transaction(IPCMSG* msg, UInt32 timeout);
Bool IPC_Send_datagram(IPCMSG* msg, UInt32 timeout);

Bool IPC_Send_message(IPCMSG* msg, UChar type, UChar acknak, UInt32 timeout);

#endif /* _IPCTASK_H_ */
