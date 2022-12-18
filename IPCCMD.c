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

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Assert.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/gates/GateMutex.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* XDCtools Header files */
#include "Board.h"
#include "IPCCMD.h"

/*****************************************************************************
 * Default Register Configuration Data (all outputs)
 *****************************************************************************/

/* Default IPCCMD parameters structure */
const IPCCMD_Params IPCCMD_defaultParams = {
    .dummy = 0,
};

/* Static Function Prototypes */

/*****************************************************************************
 * Initialize default driver parameters
 *****************************************************************************/

Void IPCCMD_Params_init(
        IPCCMD_Params *params
        )
{
    Assert_isTrue(params != NULL, NULL);

    *params = IPCCMD_defaultParams;
}

/*****************************************************************************
 * Construct the driver object
 *****************************************************************************/

IPCCMD_Handle IPCCMD_construct(
        IPCCMD_Object *obj,
        UART_Handle uartHandle,
        IPCCMD_Params *params
        )
{
    /* Initialize the object members */
    obj->uartHandle = uartHandle;

    IPC_FrameInit(&(obj->txFCB));
    IPC_FrameInit(&(obj->rxFCB));

    /* Initialize object data members */
#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_construct(&(obj->gate), NULL);
#endif
    return (IPCCMD_Handle)obj;
}

/*****************************************************************************
 * Destruct the driver object
 *****************************************************************************/

Void IPCCMD_destruct(
        IPCCMD_Handle handle
        )
{
    Assert_isTrue((handle != NULL), NULL);

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_destruct(&(handle->gate));
#endif
}

/*****************************************************************************
 * Create Handle to I/O expander and initialize it
 *****************************************************************************/

IPCCMD_Handle IPCCMD_create(
        UART_Handle uartHandle,
        IPCCMD_Params *params
        )
{
    IPCCMD_Handle handle;
    IPCCMD_Object* obj;
    Error_Block eb;

    Error_init(&eb);

    obj = Memory_alloc(NULL, sizeof(IPCCMD_Object), NULL, &eb);

    if (obj == NULL)
        return NULL;

    handle = IPCCMD_construct(obj, uartHandle, params);

    return handle;
}

/*****************************************************************************
 * Destruct and free memory
 *****************************************************************************/

Void IPCCMD_delete(
        IPCCMD_Handle handle
        )
{
    IPCCMD_destruct(handle);

    Memory_free(NULL, handle, sizeof(IPCCMD_Object));
}

/*****************************************************************************
 * CLIENT SIDE FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * This function sends a message to a server, but doesn't expect any response
 * message or ack/nak. Note the caller must set "msglen" the header to
 * to specify the message size.
 *
 * request->msglen must be set to specify tx message buffer size!
 *****************************************************************************/

int IPCCMD_Notify(
        IPCCMD_Handle handle,
        IPCMSG_HDR* request
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    /* Setup FCB for message only type frame. The request and
     * reply message lengths must already be set by caller.
     */

    handle->txFCB.type   = IPC_MAKETYPE(IPC_F_DATAGRAM, IPC_MSG_ONLY);
    handle->txFCB.acknak = 0;

    /* Send IPC command/data to track controller */
    rc = IPC_FrameTx(handle->uartHandle, &(handle->txFCB), request, request->msglen);

    if (rc == IPC_ERR_SUCCESS)
    {
        /* increment sequence number on reply */
        handle->txFCB.seqnum = IPC_INC_SEQ(handle->txFCB.seqnum);
    }

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

/*****************************************************************************
 * request->msglen must be set to specify tx message buffer size!
 * reply->msglen must be set to specify maximum rx message buffer size!
 *****************************************************************************/

int IPCCMD_Transaction(
        IPCCMD_Handle handle,
        IPCMSG_HDR* request,
        IPCMSG_HDR* reply
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    /* Setup FCB for message only type frame. The request and
     * reply message lengths must already be set by caller.
     */

    handle->txFCB.type   = IPC_MAKETYPE(0, IPC_MSG_ONLY);
    handle->txFCB.acknak = 0;

    /* Send IPC command/data to track controller */
    rc = IPC_FrameTx(handle->uartHandle, &(handle->txFCB), request, request->msglen);

    if (rc == IPC_ERR_SUCCESS)
    {
        /* Try to read ack/nak response back */
        rc = IPC_FrameRx(handle->uartHandle, &(handle->rxFCB), reply, &(reply->msglen));

        if (rc == IPC_ERR_SUCCESS)
        {
            /* increment sequence number on reply */
            handle->txFCB.seqnum = IPC_INC_SEQ(handle->txFCB.seqnum);
        }
    }

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

/*****************************************************************************
 * SERVER SIDE FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * reply->msglen must be set to specify maximum rx message buffer size!
 *****************************************************************************/

int IPCCMD_ReadMessage(
        IPCCMD_Handle handle,
        IPCMSG_HDR* request
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    /* Try to read ack/nak response back */
    rc = IPC_FrameRx(handle->uartHandle, &(handle->rxFCB), request, &(request->msglen));

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

/*****************************************************************************
 * reply->msglen must be set to specify tx message length!
 *****************************************************************************/

int IPCCMD_WriteMessage(
        IPCCMD_Handle handle,
        IPCMSG_HDR* reply
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    handle->txFCB.type   = IPC_MAKETYPE(0, IPC_MSG_ONLY);
    handle->txFCB.seqnum = handle->rxFCB.seqnum;

    /* Send IPC command/data to track controller */
    rc = IPC_FrameTx(handle->uartHandle, &(handle->txFCB), reply, reply->msglen);

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

/*****************************************************************************
 * reply->msglen must be set to specify tx message length!
 *****************************************************************************/

int IPCCMD_WriteMessageACK(
        IPCCMD_Handle handle,
        IPCMSG_HDR* reply
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    handle->txFCB.type   = IPC_MAKETYPE(0, IPC_MSG_ACK);
    handle->txFCB.acknak = handle->rxFCB.seqnum;
    handle->txFCB.seqnum = handle->rxFCB.seqnum;

    /* Send IPC command/data to track controller */
    rc = IPC_FrameTx(handle->uartHandle, &(handle->txFCB), reply, reply->msglen);

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

/*****************************************************************************
 * reply->msglen must be set to specify tx message length!
 *****************************************************************************/

int IPCCMD_WriteMessageNAK(
        IPCCMD_Handle handle,
        IPCMSG_HDR* reply
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    handle->txFCB.type   = IPC_MAKETYPE(IPC_F_ERROR, IPC_MSG_NAK);
    handle->txFCB.acknak = handle->rxFCB.seqnum;
    handle->txFCB.seqnum = handle->rxFCB.seqnum;

    /* Send IPC command/data to track controller */
    rc = IPC_FrameTx(handle->uartHandle, &(handle->txFCB), reply, reply->msglen);

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

/*****************************************************************************
 *
 *****************************************************************************/

int IPCCMD_WriteACK(
        IPCCMD_Handle handle
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    /* Transmit a NAK error response back to the client */
    handle->txFCB.type   = IPC_MAKETYPE(IPC_F_ACKNAK, IPC_ACK_ONLY);
    handle->txFCB.acknak = handle->rxFCB.seqnum;

    /* Transmit the NAK with error flag bit set */
    rc = IPC_FrameTx(handle->uartHandle, &(handle->txFCB), NULL, 0);

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

/*****************************************************************************
 *
 *****************************************************************************/

int IPCCMD_WriteNAK(
        IPCCMD_Handle handle
        )
{
    int rc;

#if (IPCCMD_THREAD_SAFE > 0)
    IArg key = GateMutex_enter(GateMutex_handle(&(handle->gate)));
#endif

    /* Transmit a NAK error response back to the client */
    handle->txFCB.type   = IPC_MAKETYPE(IPC_F_ACKNAK | IPC_F_ERROR, IPC_NAK_ONLY);
    handle->txFCB.acknak = handle->rxFCB.seqnum;

    /* Transmit the NAK with error flag bit set */
    rc = IPC_FrameTx(handle->uartHandle, &(handle->txFCB), NULL, 0);

#if (IPCCMD_THREAD_SAFE > 0)
    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
#endif

    return rc;
}

// End-Of-File
