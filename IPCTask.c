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
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>

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

#include "DTC1200.h"
#include "Globals.h"
#include "RAMP.h"
#include "IPCTask.h"

/* External Data Items */

/* Global Data Items */

UART_Handle uartHandle;
FCB g_rxFcb;
FCB g_txFcb;

/* The following objects are created statically. */
extern Semaphore_Handle sem;
extern Queue_Handle msgQueue;
extern Queue_Handle g_txQueue;

/* Static Function Prototypes */

//*****************************************************************************
//
//*****************************************************************************

int IPC_init(void)
{
    Int i;
    IPCMSG *msg;
    Error_Block eb;
    UART_Params uartParams;

    Error_init(&eb);

    msg = (IPCMSG *)Memory_alloc(NULL, NUMMSGS * sizeof(IPCMSG), 0, &eb);

    if (msg == NULL)
        System_abort("Memory allocation failed");

    /* Put all messages on freeQueue */
    for (i=0; i < NUMMSGS; msg++, i++)
        Queue_put(g_txQueue, (Queue_Elem *)msg);

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

    uartHandle = UART_open(Board_UART_IPC, &uartParams);

    if (uartHandle == NULL)
        System_abort("Error initializing UART\n");

    return 1;
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCReaderTaskFxn(UArg arg0, UArg arg1)
{
    int rc;
    uint8_t rxBuf[16];
    IPCMSG msg;

    /* Now begin the main program command task processing loop */

    RAMP_InitFcb(&g_rxFcb);

    while (true)
    {
        rc = RAMP_RxFrame(uartHandle, &g_rxFcb, rxBuf, sizeof(rxBuf));

        //msg = (IPCMSG *)Queue_get(freeQueue);

        /* Increment the frame sequence number */
        //g_RxFcb.seqnum = INC_SEQ_NUM(g_RxFcb.seqnum);
    }
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCWriterTaskFxn(UArg arg0, UArg arg1)
{
    int rc;
    uint8_t txBuf[16];

    /* Now begin the main program command task processing loop */

    RAMP_InitFcb(&g_txFcb);

    while (true)
    {
        /* Wait for semaphore to be posted by writer(). */
        Semaphore_pend(sem, BIOS_WAIT_FOREVER);

        rc = RAMP_TxFrame(uartHandle, &g_txFcb, txBuf, sizeof(txBuf));

        /* Increment the frame sequence number */
        //g_TxFcb.seqnum = INC_SEQ_NUM(g_RxFcb.seqnum);
    }
}

// End-Of-File
