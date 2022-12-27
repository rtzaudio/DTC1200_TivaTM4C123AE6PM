/* ============================================================================
 *
 * IPC Command Messaging Functions v1.02
 *
 * Copyright (C) 2022, RTZ Professional Audio, LLC
 * All Rights Reserved
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

#ifndef _IPCCMD_H_
#define _IPCCMD_H_

/* Set to one for thread safe use */
#ifndef IPCCMD_THREAD_SAFE
#define IPCCMD_THREAD_SAFE  1
#endif

#include <stdint.h>
#include <stdbool.h>
#include <ti/drivers/UART.h>
#if (IPCCMD_THREAD_SAFE > 0)
#include <ti/sysbios/gates/GateMutex.h>
#endif

#include "IPCFrame.h"

/*****************************************************************************
 * Driver Data Structures
 *****************************************************************************/

/* IPCCMD Parameters object points to init data */
typedef struct IPCCMD_Params {
    UART_Handle         uartHandle;
} IPCCMD_Params;

/* IPCCMD handle object */
typedef struct IPCCMD_Object {
    UART_Handle      	uartHandle;         /* handle for SPI object   */
    IPC_FCB             txFCB;
    IPC_FCB             rxFCB;
#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_Struct    gate;
#endif
} IPCCMD_Object;

/* Handle to I/O expander */
typedef IPCCMD_Object* IPCCMD_Handle;

/*****************************************************************************
 * IPC Message Header Structure
 *****************************************************************************/

typedef struct _IPCMSG_HDR {
    uint16_t    opcode;                     /* the IPC message type code   */
    uint16_t    msglen;                     /* msg length + payload length */
} IPCMSG_HDR;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

Void IPCCMD_Params_init(
        IPCCMD_Params *params
        );

IPCCMD_Handle IPCCMD_create(
        IPCCMD_Params *params
        );

void IPCCMD_delete(
        IPCCMD_Handle handle
        );

IPCCMD_Handle IPCCMD_construct(
        IPCCMD_Object *obj,
        IPCCMD_Params *params
        );

Void IPCCMD_destruct(
        IPCCMD_Handle handle
        );

/*****************************************************************************
 * Client Side Functions
 *****************************************************************************/

int IPCCMD_Transaction(
        IPCCMD_Handle handle,
        IPCMSG_HDR* request,
        IPCMSG_HDR* reply
        );

int IPCCMD_Notify(
        IPCCMD_Handle handle,
        IPCMSG_HDR* request
        );

/*****************************************************************************
 * Server Side Functions
 *****************************************************************************/

int IPCCMD_ReadMessage(
        IPCCMD_Handle handle,
        IPCMSG_HDR* request
        );

int IPCCMD_WriteMessage(
        IPCCMD_Handle handle,
        IPCMSG_HDR* reply
        );

int IPCCMD_WriteMessageACK(
        IPCCMD_Handle handle,
        IPCMSG_HDR* reply
        );

int IPCCMD_WriteMessageNAK(
        IPCCMD_Handle handle,
        IPCMSG_HDR* reply
        );

int IPCCMD_WriteACK(
        IPCCMD_Handle handle
        );

int IPCCMD_WriteNAK(
        IPCCMD_Handle handle
        );

#endif /*_IPCCMD_H_*/
