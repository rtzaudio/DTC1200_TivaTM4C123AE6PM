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

/* PMX42 Board Header file */

//#include "STC1200.h"
#include "Board.h"
#include "IPCTask.h"

/* External Data Items */

/* Global Data Items */

static IPCSVR_OBJECT g_ipc;

/* Static Function Prototypes */

static Void IPCReaderTaskFxn(UArg a0, UArg a1);
static Void IPCWriterTaskFxn(UArg arg0, UArg arg1);

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Server_init(void)
{
    Int i;
    FCBMSG* msg;
    Error_Block eb;
    UART_Params uartParams;
    Task_Params taskParams;

    /* Open the UART for binary mode */

    UART_Params_init(&uartParams);

    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 1000;                   // 1 second read timeout
    uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
    uartParams.readCallback   = NULL;
    uartParams.writeCallback  = NULL;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readEcho       = UART_ECHO_OFF;
    uartParams.baudRate       = 115200;
    uartParams.stopBits       = UART_STOP_ONE;
    uartParams.parityType     = UART_PAR_NONE;

    g_ipc.uartHandle = UART_open(Board_UART_IPC, &uartParams);

    if (g_ipc.uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /* Create the queues needed */
    g_ipc.txFreeQue = Queue_create(NULL, NULL);
    g_ipc.txDataQue = Queue_create(NULL, NULL);

    /* Create semaphores needed */
    g_ipc.txFreeSem = Semaphore_create(MAX_WINDOW, NULL, NULL);
    g_ipc.txDataSem = Semaphore_create(0, NULL, NULL);

    /* Allocate Transmit Buffer Memory */

    Error_init(&eb);

    if ((g_ipc.txMsgBuf = (FCBMSG*)Memory_alloc(NULL, MAX_WINDOW * sizeof(FCBMSG), 0, &eb)) == NULL)
        System_abort("TxBuf allocation failed");

    msg = g_ipc.txMsgBuf;

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < MAX_WINDOW; i++, msg++) {
        Queue_enqueue(g_ipc.txFreeQue, (Queue_Elem*)msg);
    }

    /* Allocate Single Receive Buffer Memory */

    Error_init(&eb);

    if ((g_ipc.rxMsgBuf = (FCBMSG*)Memory_alloc(NULL, sizeof(FCBMSG), 0, &eb)) == NULL)
        System_abort("RxBuf allocation failed");

    /* Initialize all Server Data Items */

    g_ipc.numFreeMsgs = MAX_WINDOW;
    g_ipc.currSeq     = MIN_SEQ_NUM;	/* current tx sequence# */
    g_ipc.expectSeq   = 1;     			/* expected rx seq#     */
    g_ipc.lastSeq     = 0;     			/* last seq# rx'ed      */
    Error_init(&eb);

    /* Create the reader/writer tasks */

    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 5;
    Task_create((Task_FuncPtr)IPCWriterTaskFxn, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 6;
    Task_create((Task_FuncPtr)IPCReaderTaskFxn, &taskParams, &eb);

    return TRUE;
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Send_datagram(IPCMSG* msg, UInt32 timeout)
{
    UChar type = MAKETYPE(F_DATAGRAM, TYPE_MSG_ONLY);

    return IPC_Send_message(msg, type, 0, timeout);
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Send_transaction(IPCMSG* msg, UInt32 timeout)
{
    UChar type = MAKETYPE(0, TYPE_MSG_ONLY);

    return IPC_Send_message(msg, type, 0, timeout);
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Send_message(IPCMSG* msg, UChar type, UChar acknak, UInt32 timeout)
{
    UInt key;
    FCBMSG* elem;

    if (Semaphore_pend(g_ipc.txFreeSem, timeout))
    {
        /* perform the dequeue and decrement numFreeMsgs atomically */
        key = Hwi_disable();

        /* get a message from the free queue */
        elem = Queue_dequeue(g_ipc.txFreeQue);

        /* Make sure that a valid pointer was returned. */
        if (elem == (FCBMSG*)(g_ipc.txFreeQue))
        {
            Hwi_restore(key);
            return FALSE;
        }

        /* decrement the numFreeMsgs */
        g_ipc.numFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* copy msg to element */
        if (msg)
            memcpy(&(elem->msg), msg, sizeof(IPCMSG));

        elem->fcb.type    = type;       /* frame type bits       */
        elem->fcb.acknak  = acknak;     /* frame ACK/NAK seq#    */
        elem->fcb.address = 0;

        /* put message on dataQueue */
        Queue_put(g_ipc.txDataQue, (Queue_Elem *)elem);

        /* post the semaphore */
        Semaphore_post(g_ipc.txDataSem);

        return TRUE;          /* success */
    }

    return FALSE;         /* error */
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCWriterTaskFxn(UArg arg0, UArg arg1)
{
    UInt key;
    FCBMSG* elem;

    /* Now begin the main program command task processing loop */

    while (TRUE)
    {
        /* Wait for a packet in the tx queue */
    	if (!Semaphore_pend(g_ipc.txDataSem, 1000))
    	{
    	    /* Timeout, nothing to send */
    	    continue;
    	}

    	/* get message from dataQue */
        elem = Queue_get(g_ipc.txDataQue);

        elem->fcb.textbuf = &(elem->msg);
        elem->fcb.textlen = sizeof(IPCMSG);

        /* Set the sequence number prior to transmit */
        elem->fcb.seqnum = g_ipc.currSeq;

        /* Transmit the packet! */
        RAMP_TxFrame(g_ipc.uartHandle, &(elem->fcb));

        /* Perform the enqueue and increment numFreeMsgs atomically */
        key = Hwi_disable();

        /* Put message buffer back on the free queue */
        Queue_enqueue(g_ipc.txFreeQue, (Queue_Elem *)elem);

        /* Increment numFreeMsgs */
        g_ipc.numFreeMsgs++;

        /* Increment total number of packets transmitted */
        g_ipc.txCount++;

        /* Increment the servers tx sequence number */
        g_ipc.currSeq = INC_SEQ_NUM(g_ipc.currSeq);

        /* re-enable ints */
        Hwi_restore(key);

        /* post the semaphore */
        Semaphore_post(g_ipc.txFreeSem);
    }
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCReaderTaskFxn(UArg arg0, UArg arg1)
{
    int rc;

    FCB *fcb = &(g_ipc.rxMsgBuf->fcb);

    /* Now begin the main program command task processing loop */

    while (true)
    {
    	fcb->textbuf = &g_ipc.rxMsgBuf->msg;
    	fcb->textlen = sizeof(IPCMSG);

        rc = RAMP_RxFrame(g_ipc.uartHandle, fcb);

        if (rc == ERR_TIMEOUT)
        	continue;

        if (rc != 0)
        {
        	System_printf("RAMP_RxFrame Error %d\n", rc);
        	System_flush();
        	continue;
        }

        /* Save the last sequence number received */
        g_ipc.lastSeq = fcb->seqnum;

        /* Increment the total packets received count */
        g_ipc.rxCount++;

        /* We've received a valid frame, attempt to decode it */

        switch(fcb->type & FRAME_TYPE_MASK)
        {
        	case TYPE_ACK_ONLY:			/* ACK message frame only      */
        		break;

        	case TYPE_NAK_ONLY:			/* NAK message frame only      */
        		break;

        	case TYPE_MSG_ONLY:			/* message only frame          */

        		//if (fcb->type & F_DATAGRAM)
        		{
                	System_printf("IPC Rx: %04x : %04x\n",
                			g_ipc.rxMsgBuf->msg.type,
							g_ipc.rxMsgBuf->msg.opcode);

                	System_flush();
        		}
        		break;

        	case TYPE_MSG_ACK:			/* piggyback message plus ACK  */
        		break;

        	case TYPE_MSG_NAK:			/* piggyback message plus NAK  */
        		break;

        	default:
        		break;
        }
    }
}

// End-Of-File
