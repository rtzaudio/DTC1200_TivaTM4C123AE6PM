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

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/Memory.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/gates/GateMutex.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* Tivaware Driver files */
#include <driverlib/eeprom.h>
#include <driverlib/fpu.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* XDCtools Header files */
#include "DTC1200.h"
#include "Globals.h"
#include "ServoTask.h"
#include "TransportTask.h"
#include "IPCFromSTCTask.h"
#include "IPCCMD.h"
#include "IPCCMD_DTC1200.h"


#define RXBUFSIZ    (sizeof(SYSPARMS) + 64)

/* Static Function Prototypes */
static Void IPCFromSTC_Task(UArg a0, UArg a1);
static int HandleVersion(IPCCMD_Handle handle, DTC_IPCMSG_VERSION_GET* msg);
static int HandleEPROM(IPCCMD_Handle handle, DTC_IPCMSG_CONFIG_EPROM* msg);
static int HandleConfigSet(IPCCMD_Handle handle, DTC_IPCMSG_CONFIG_SET* msg);
static int HandleConfigGet(IPCCMD_Handle handle, DTC_IPCMSG_CONFIG_GET* msg);
static int HandleTransportCmd(IPCCMD_Handle handle, DTC_IPCMSG_TRANSPORT_CMD* msg);

//*****************************************************************************
// Main Program Entry Point
//*****************************************************************************

Int IPCFromSTC_Startup(void)
{
    Error_Block eb;
    Task_Params taskParams;

    /* Start the main application button polling task */
    Error_init(&eb);

    Task_Params_init(&taskParams);

    taskParams.stackSize = 1248;
    taskParams.priority  = 13;

    if (Task_create(IPCFromSTC_Task, &taskParams, &eb) == NULL)
        System_abort("MainTask!\n");

    return 0;
}

//*****************************************************************************
// This task handles IPC messages from the STC via Board_UART_IPC_B.
//*****************************************************************************

Void IPCFromSTC_Task(UArg a0, UArg a1)
{
    int rc;
    uint8_t* msgBuf;
    Error_Block eb;
    UART_Params uartParams;
    UART_Handle uartHandle;
    IPCCMD_Handle ipcHandle;
    IPCCMD_Params ipcParams;

    /* Open the UART for binary mode */

    UART_Params_init(&uartParams);

    /* RS-232 port-B 115200,N,8,1 with 2-sec read timeout */
    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 2000;
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

    uartHandle = UART_open(Board_UART_IPC_B, &uartParams);

    if (uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /* Initialize default IPC command parameters */
    IPCCMD_Params_init(&ipcParams);

    /* Attach UART to IPC command object */
    ipcParams.uartHandle = uartHandle;

    /* Create the IPC command object */
    ipcHandle = IPCCMD_create(&ipcParams);

    if (ipcHandle == NULL)
        System_abort("IPCCMD_create() failed");

    /****************************************************************
     * Enter the main application button processing loop forever.
     ****************************************************************/

    Error_init(&eb);

    msgBuf = (uint8_t*)Memory_alloc(NULL, RXBUFSIZ, 0, &eb);

    if (msgBuf == NULL)
        System_abort("RxBuf allocation failed");

    for(;;)
    {
        /* Get pointer to header in receive message buffer */
        DTC_IPCMSG_HDR* msg = (DTC_IPCMSG_HDR*)msgBuf;

        /* Specifies the max buffer size our receiver can hold.
         * This gets updated on return and contains the actual
         * number of header and message bytes received.
         */
        msg->msglen = RXBUFSIZ;

        /* Attempt to receive an IPC message */
        if ((rc = IPCCMD_ReadMessage(ipcHandle, msg)) == IPC_ERR_TIMEOUT)
            continue;

        /* Check for any error reading a packet */
        if (rc != IPC_ERR_SUCCESS)
        {
            System_printf("IPCCMD_ReadMessage() error %d\n", rc);
            System_flush();
            continue;
        }

        /*
         * Dispatch IPC message by opcode
         */

        switch(msg->opcode)
        {
        case DTC_OP_VERSION_GET:
            /* Return the current firmware version and build */
            rc = HandleVersion(ipcHandle, (DTC_IPCMSG_VERSION_GET*)msg);
            break;

        case DTC_OP_CONFIG_EPROM:
            /* Read or Write the global configuration data to EPROM */
            rc = HandleEPROM(ipcHandle, (DTC_IPCMSG_CONFIG_EPROM*)msg);
            break;

        case DTC_OP_CONFIG_GET:
            /* Get the global configuration data */
            rc = HandleConfigGet(ipcHandle, (DTC_IPCMSG_CONFIG_GET*)msg);
            break;

        case DTC_OP_CONFIG_SET:
            /* Set the global configuration data */
            rc = HandleConfigSet(ipcHandle, (DTC_IPCMSG_CONFIG_SET*)msg);
            break;

        case DTC_OP_TRANSPORT_CMD:
            /* Issue a transport command */
            rc =  HandleTransportCmd(ipcHandle, (DTC_IPCMSG_TRANSPORT_CMD*)msg);
            break;

        default:
            /* Transmit a NAK error response to client */
            rc = IPCCMD_WriteNAK(ipcHandle);
            break;
        }
    }
}

//*****************************************************************************
// Return the current DTC firmware version and build number
//*****************************************************************************

int HandleVersion(
        IPCCMD_Handle handle,
        DTC_IPCMSG_VERSION_GET* msg
        )
{
    int rc = 0;

    msg->version = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    msg->build   = FIRMWARE_BUILD;

    /* Set length of return data */
    msg->hdr.msglen = sizeof(DTC_IPCMSG_VERSION_GET);

    /* Write message plus ACK to client */
    rc = IPCCMD_WriteMessageACK(handle, &msg->hdr);

    return rc;
}

//*****************************************************************************
// The method handles storing the current system configuration data to or
// from EPROM memory. It also allows resetting all parameters to their
// default values.
//*****************************************************************************

int HandleEPROM(
        IPCCMD_Handle handle,
        DTC_IPCMSG_CONFIG_EPROM* msg
        )
{
    int rc = 0;

    /* Read or write config parameters to EPROM */
    switch (msg->store)
    {
    case 0:
        /* Load system parameters from EPROM */
        rc = SysParamsRead(&g_sys);
        break;

    case 1:
        /* Write system parameters to EPROM */
        rc = SysParamsWrite(&g_sys);
        break;

    case 2:
        /* Reset system parameters to defaults */
        InitSysDefaults(&g_sys);
        break;

    default:
        rc = 1;
        break;
    }

    msg->status = rc;

    /* Set length of return data */
    msg->hdr.msglen = sizeof(DTC_IPCMSG_CONFIG_EPROM);

    /* Write message plus ACK to client */
    rc = IPCCMD_WriteMessageACK(handle, &msg->hdr);

    return rc;
}

//*****************************************************************************
// This method returns all configuration data currently in runtime memory.
// The EPROM configuration data stored may be different from what is in
// runtime memory if the user has made any setting changes.
//*****************************************************************************

int HandleConfigGet(
        IPCCMD_Handle handle,
        DTC_IPCMSG_CONFIG_GET* msg
        )
{
    int rc;

    /* Copy the global configuration data from the global memory
     * buffer to the transmit message buffer and send it.
     */
    memcpy(&(msg->cfg), &g_sys, sizeof(msg->cfg));

    /* Set length of return data */
    msg->hdr.msglen = sizeof(DTC_IPCMSG_CONFIG_GET);

    /* Write config data plus ACK back to client */
    rc = IPCCMD_WriteMessageACK(handle, &msg->hdr);

    return rc;
}

//*****************************************************************************
// This method replaces the entire configuration set in runtime memory. All
// parameters are replaced in runtime memory only. You must issue a config
// EPROM write command to store the configuration in EPROM. The configuration
// data is loaded from EPROM each time the system begins execution.
//*****************************************************************************

int HandleConfigSet(
        IPCCMD_Handle handle,
        DTC_IPCMSG_CONFIG_SET* msg
        )
{
    int rc;

    /* Copy the configuration data from the message receive buffer
     * to the global config memory buffer and send ACK to client.
     */
    memcpy(&g_sys, &(msg->cfg), sizeof(msg->cfg));

    /* Write ACK back to client */
    rc = IPCCMD_WriteACK(handle);

    return rc;
}

//*****************************************************************************
// This method queues transport commands (eg, stop, play, record, fwd, rew).
// Some commands support flag options in param1 and param2, see comments below.
//*****************************************************************************

int HandleTransportCmd(
        IPCCMD_Handle handle,
        DTC_IPCMSG_TRANSPORT_CMD* msg
        )
{
    int rc;
    uint16_t param1 = (uint16_t)msg->param1;
    uint16_t param2 = (uint16_t)msg->param2;

    if (Servo_IsMode(MODE_HALT))
        return 0;

    switch(msg->cmd)
    {
    case DTC_Transport_STOP:
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_STOP, 0);
        break;

    case DTC_Transport_PLAY:
        /* param1 is zero, otherwise it specifies M_RECORD for record mode */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_PLAY | (param1 & M_RECORD), 0);
        break;

    case DTC_Transport_FWD:
        /* param1 is zero, otherwise it specifies the velocity */
        /* param2 is zero, otherwise it specifies flags: M_LIBWIND|M_NOSLOW */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_FWD | (param2 & (M_LIBWIND|M_NOSLOW)), param1);
        break;

    case DTC_Transport_FWD_LIB:
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_FWD | M_LIBWIND | M_NOSLOW, 0);
        break;

    case DTC_Transport_REW:
        /* param1 is zero, otherwise it specifies the velocity */
        /* param2 is zero, otherwise it specifies flags: M_LIBWIND|M_NOSLOW */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_REW | (param2 & (M_LIBWIND|M_NOSLOW)), param1);
        break;

    case DTC_Transport_REW_LIB:
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_REW | M_LIBWIND | M_NOSLOW, 0);
        break;

    default:
        break;
    }

    /* Write ACK back to client */
    rc = IPCCMD_WriteACK(handle);

    return rc;
}

/* End-Of-File */

