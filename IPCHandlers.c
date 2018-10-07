/*
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
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/BIOS.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Queue.h>

/* Board Header file */
#include "Board.h"
#include "DTC1200.h"
#include "Globals.h"
#include "IPCServer.h"
#include "ServoTask.h"
#include "TransportTask.h"

/* Static Function Prototypes */
static void DispatchTransport(IPCMSG* msg, IPCMSG* reply);
static void DispatchConfig(IPCMSG* msg, IPCMSG* reply);

//*****************************************************************************
// This handler processes application specific datagram messages received
// from the peer. No response is required for datagrams.
//*****************************************************************************

Bool IPC_Handle_datagram(IPCMSG* msg, RAMP_FCB* fcb)
{
    uint8_t bits;

    if (msg->type == IPC_TYPE_NOTIFY)
    {
        switch(msg->opcode)
        {
        case OP_NOTIFY_BUTTON:      /* DRC transport button(s) pressed */
            bits = (uint8_t)msg->param1.U;
            Mailbox_post(g_mailboxCommander, &bits, 0);
            break;
        }
    }

    return TRUE;
}

//*****************************************************************************
// This handler processes application specific transaction based messages
// that require a MSG+ACK response.
//*****************************************************************************

Bool IPC_Handle_transaction(IPCMSG* msg, RAMP_FCB* fcb, UInt32 timeout)
{
    RAMP_FCB fcbReply;
    IPCMSG msgReply;

    /* Copy incoming message to reply as default values */
    memcpy(&msgReply, msg, sizeof(IPCMSG));

    /* Execute the transaction for request */
    //System_printf("Xact(%d) Begin: %d %02x\n", fcb->seqnum, msg->opcode, msg->param1.U);
    //System_flush();

    switch(msg->type)
    {
    case IPC_TYPE_CONFIG:
        DispatchConfig(msg, &msgReply);
        break;

    case IPC_TYPE_XPORT:
        DispatchTransport(msg, &msgReply);
        break;

    default:
        msgReply.param1.U = 0;
        msgReply.param2.U = 0;
        break;
    }

    /* Send the response msg+ack with command results returned */

    fcbReply.type    = MAKETYPE(F_ACKNAK, TYPE_MSG_ACK);
    fcbReply.acknak  = fcb->seqnum;
    fcbReply.address = fcb->address;
    fcbReply.seqnum  = IPC_GetTxSeqNum();

    return IPC_Message_post(&msgReply, &fcbReply, timeout);
}

//*****************************************************************************
// DISPATCH TRANSPORT MODE CONTROL REQUESTS
//*****************************************************************************

void DispatchTransport(IPCMSG* msg, IPCMSG* reply)
{
    uint16_t param1 = (uint16_t)msg->param1.U;

    switch(msg->opcode)
    {
    case OP_MODE_STOP:
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_STOP, 0);
        break;

    case OP_MODE_PLAY:
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_PLAY, 0);
        break;

    case OP_MODE_FWD:
        /* param1 is zero, otherwise it specifies the velocity */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_FWD, param1);
        break;

    case OP_MODE_FWD_LIB:
        /* param1 is zero, otherwise it specifies the velocity */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_FWD | M_LIBWIND, 0);
        break;

    case OP_MODE_REW:
        /* param1 is zero, otherwise it specifies the velocity */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_REW, param1);
        break;

    case OP_MODE_REW_LIB:
        /* param1 is zero, otherwise it specifies the velocity */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_REW | M_LIBWIND, 0);
        break;

    default:
        break;
    }
}

//*****************************************************************************
// DISPATCH CONFIGURATION GET/SET REQUESTS
//*****************************************************************************

void DispatchConfig(IPCMSG* msg, IPCMSG* reply)
{
    switch(msg->opcode)
    {
    case OP_GET_SHUTTLE_VELOCITY:
        reply->param1.U = (int16_t)g_sys.shuttle_velocity;
        break;

    case OP_SET_SHUTTLE_VELOCITY:
        g_sys.shuttle_velocity = (int32_t)msg->param1.U;
        break;

    default:
        break;
    }
}

// End-Of-File
