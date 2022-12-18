/***************************************************************************
 *
 * IPC Low-level Tx/Rx Data Frame Functions - v1.01
 *
 * Copyright (C) 2016-2021, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************
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
 *
 ***************************************************************************/

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
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

#include <driverlib/sysctl.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "CRC16.h"
#include "IPCFrame.h"

//*****************************************************************************
//
// Name:        IPC_FrameInit()
//
// Synopsis:    void IPC_FrameInit(fcb)
//
//              IPC_FCB*    fcb     - Ptr to frame control block
//
// Description: Initialize FCB structure with some defaults.
//
// Return:      void
//
//*****************************************************************************

void IPC_FrameInit(IPC_FCB* fcb)
{
    fcb->type      = IPC_MAKETYPE(0, IPC_MSG_ONLY);
    fcb->seqnum    = IPC_MIN_SEQ;
    fcb->acknak    = 0;
    fcb->rsvd      = 0;
}

//*****************************************************************************
//
// Name:        IPC_FrameRx()
//
// Synopsis:    int IPC_FrameRx(handle, fcb, txtbuf, txtlen)
//
//              UART_Handle handle  - UART handle
//
//              IPC_FCB*    fcb     - Ptr to frame control block
//
//              void*       txtbuf  - Ptr to msg txt rx buffer
//
//              uint16_t*   txtlen  - Specifies the max rx buffer on entry and
//                                    returns actual message length received
//                                    on return.
//
// Description: Receive an IPC frame from the serial port. On entry the value
//              pointed to by txtlen contains the maximum size of the rx
//              message buffer - this is used to check for overflow.
//              Upon return, it contains the actual length of the message
//              received. If no message or ACK only message, it contains zero.
//
// Return:      Returns IPC_ERR_SUCCESS on success, otherwise error code.
//
//*****************************************************************************

int IPC_FrameRx(
        UART_Handle handle,
        IPC_FCB*    fcb,
        void*       txtbuf,
        uint16_t*   txtlen
        )
{
    int i;
    int rc = IPC_ERR_SUCCESS;
    uint8_t b;
    uint8_t type;
    uint16_t lsb;
    uint16_t msb;
    uint16_t framelen;
    uint16_t rxcrc;
    uint16_t crc = 0;

    uint8_t *textbuf = txtbuf;
    uint16_t textlen = *txtlen;

    /* No message text bytes received yet */
    *txtlen = 0;

    /* First, try to synchronize to 0x79 SOF byte */

    i = 0;

    do {

        /* Read the preamble MSB for the frame start */
        if (UART_read(handle, &b, 1) != 1)
            return IPC_ERR_TIMEOUT;

        /* Garbage flood check, synch lost?? */
        if (i++ > (IPC_FRAME_OVERHEAD + IPC_PREAMBLE_OVERHEAD + IPC_MAX_TEXT_LEN))
            return IPC_ERR_SYNC;

    } while (b != IPC_PREAMBLE_MSB);

    /* We found the first 0x79 sequence, next byte
     * has to be 0xFC for a valid SOF sequence.
     */

    if (UART_read(handle, &b, 1) != 1)
        return IPC_ERR_TIMEOUT;

    if (b != IPC_PREAMBLE_LSB)
        return IPC_ERR_SYNC;

    /* Got the preamble word */

    /* CRC starts here, sum in the seed byte first */
    crc = CRC16Update(crc, IPC_CRC_SEED_BYTE);

    /* Read the Frame length (MSB) */
    if (UART_read(handle, &b, 1) != 1)
        return IPC_ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    msb = (uint16_t)b;

    /* Read the Frame length (LSB) */
    if (UART_read(handle, &b, 1) != 1)
        return IPC_ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    lsb = (uint16_t)b;

    /* Build and validate maximum frame length */
    framelen = (size_t)((msb << 8) | lsb) & 0xFFFF;

    if (framelen > IPC_MAX_FRAME_LEN)
        return IPC_ERR_FRAME_LEN;

    /* Read the Frame Type Byte */
    if (UART_read(handle, &b, 1) != 1)
        return IPC_ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    fcb->type = b;

    /* Check for ACK/NAK only frame (type always 11H or 12H) */

    /* Get the frame type less any flag bits */
    type = (fcb->type & IPC_TYPE_MASK);

    if ((type == IPC_ACK_ONLY) || (type == IPC_NAK_ONLY))
    {
        /* If ACK/NAK only, length must be ACK_FRAME_LEN */
        if (framelen != IPC_ACK_FRAME_LEN)
            return IPC_ERR_ACK_LEN;

        /* Read the ACK/NAK Sequence Number */
        if (UART_read(handle, &b, 1) != 1)
            return IPC_ERR_SHORT_FRAME;

        crc = CRC16Update(crc, b);
        fcb->acknak = b;
    }
    else
    {
        /* It's a full IPC frame, continue decoding the rest of the frame */

        /* Read the Frame Sequence Number */
        if (UART_read(handle, &b, 1) != 1)
            return IPC_ERR_SHORT_FRAME;

        crc = CRC16Update(crc, b);
        fcb->seqnum = b;

        /* Read the ACK/NAK Sequence Number */
        if (UART_read(handle, &b, 1) != 1)
            return IPC_ERR_SHORT_FRAME;

        crc = CRC16Update(crc, b);
        fcb->acknak = b;

        /* Read the Text length (MSB) */
        if (UART_read(handle, &b, 1) != 1)
            return IPC_ERR_SHORT_FRAME;

        crc = CRC16Update(crc, b);
        msb = (uint16_t)b;

        /* Read the Text length (LSB) */
        if (UART_read(handle, &b, 1) != 1)
            return IPC_ERR_SHORT_FRAME;

        crc = CRC16Update(crc, b);
        lsb = (uint16_t)b;

        /* Get the frame length received and validate it */
        uint16_t rxtextlen = (size_t)((msb << 8) | lsb) & 0xFFFF;

        /* Return the message text length received */
        *txtlen = rxtextlen;

        /* The text length should be the frame overhead minus the preamble overhead
         * plus the text length specified in the received frame. If these don't match
         * then we have either a packet data error or a malformed packet.
         */
        if (rxtextlen + (IPC_FRAME_OVERHEAD - IPC_PREAMBLE_OVERHEAD) != framelen)
            return IPC_ERR_TEXT_LEN;

        /* Read text data associated with the frame */

        for (i=0; i < rxtextlen; i++)
        {
            if (UART_read(handle, &b, 1) != 1)
                return IPC_ERR_SHORT_FRAME;

            /* update the CRC */
            crc = CRC16Update(crc, b);

            /* If we overflow, continue reading the packet
             * data, but don't store the data into the buffer.
             */
            if (i >= textlen)
            {
                rc = IPC_ERR_RX_OVERFLOW;
                continue;
            }

            if (textbuf)
                *textbuf++ = b;
        }
    }

    /* Read the packet CRC MSB */
    if (UART_read(handle, &b, 1) != 1)
        return IPC_ERR_SHORT_FRAME;

    msb = (uint16_t)b & 0xFF;

    /* Read the packet CRC LSB */
    if (UART_read(handle, &b, 1) != 1)
        return IPC_ERR_SHORT_FRAME;

    lsb = (uint16_t)b & 0xFF;

    /* Build and validate the CRC */
    rxcrc = (uint16_t)((msb << 8) | lsb) & 0xFFFF;

    /* Validate the CRC values match */
    if (rxcrc != crc)
        rc = IPC_ERR_CRC;

    return rc;
}

//*****************************************************************************
//
// Name:        IPC_TxFrame()
//
// Synopsis:    int IPC_TxFrame(handle, fcb, txtbuf, txtlen)
//
//              UART_Handle handle  - UART handle
//
//              IPC_FCB*    fcb     - Ptr to frame control block
//
//              uint8_t*    txtbuf  - Ptr to msg txt tx buffer
//
//              uint16_t   txtlen   - Specifies length of the message to send
//
// Description: Transmit an IPC frame from the serial port. On entry the value
//              pointed to by txtlen contains the number of message bytes
//              to transmit. This will be reset to zero if an error occurs
//              attempting to transmit the message or if an ACK only
//              frame is sent.
//
// Return:      Returns IPC_ERR_SUCCESS on success, otherwise error code.
//
//*****************************************************************************

int IPC_FrameTx(
        UART_Handle handle,
        IPC_FCB*    fcb,
        void*       txtbuf,
        uint16_t    txtlen
        )
{
    uint8_t b;
    uint8_t type;
    uint16_t i;
    uint16_t framelen;
    uint16_t crc;

    uint8_t *textbuf = txtbuf;
    uint16_t textlen = txtlen;

    /* First check the text length is valid */
    if (textlen > IPC_MAX_TEXT_LEN)
        return IPC_ERR_TEXT_LEN;

    /* Get the frame type less any flag bits */
    type = (fcb->type & IPC_TYPE_MASK);

    /* Are we sending a ACK or NAK only frame? */
    if ((type == IPC_ACK_ONLY) || (type == IPC_NAK_ONLY))
    {
        textbuf = NULL;
        textlen = 0;

        framelen = IPC_ACK_FRAME_LEN;

        /* Set the ACK/NAK flag bit */
        fcb->type |= IPC_F_ACKNAK;
    }
    else
    {
        /* Build the frame length with text length given */
        framelen = textlen + (IPC_FRAME_OVERHEAD - IPC_PREAMBLE_OVERHEAD);

        /* If message is piggyback ACK/NAK, set flag bit also */
        if ((type == IPC_MSG_ACK) || (type == IPC_MSG_NAK))
            fcb->type |= IPC_F_ACKNAK;
        else
            fcb->type &= ~(IPC_F_ACKNAK);
    }

    /* Send the preamble MSB for the frame start */
    b = IPC_PREAMBLE_MSB;
    UART_write(handle, &b, 1);

    /* Send the preamble LSB for the frame start */
    b = IPC_PREAMBLE_LSB;
    UART_write(handle, &b, 1);

    /* CRC starts here, sum in the seed byte first */
    crc = CRC16Update(0, IPC_CRC_SEED_BYTE);

    /* Send the frame length (MSB) */
    b = (uint8_t)((framelen >> 8) & 0xFF);
    crc = CRC16Update(crc, b);
    UART_write(handle, &b, 1);

    /* Send the frame length (LSB) */
    b = (uint8_t)(framelen & 0xFF);
    crc = CRC16Update(crc, b);
    UART_write(handle, &b, 1);

    /* Send the frame type & flags byte */
    b = (uint8_t)(fcb->type & 0xFF);
    crc = CRC16Update(crc, b);
    UART_write(handle, &b, 1);

    /* Sending ACK or NAK only frame? */

    if ((type == IPC_ACK_ONLY) || (type == IPC_NAK_ONLY))
    {
        /* Sending ACK/NAK frame only  */

        b = (uint8_t)(fcb->acknak & 0xFF);
        crc = CRC16Update(crc, b);
        UART_write(handle, &b, 1);
    }
    else
    {
        /* Continue sending a full IPC frame */

        /* Send the Frame Sequence Number */
        b = (uint8_t)(fcb->seqnum & 0xFF);
        crc = CRC16Update(crc, b);
        UART_write(handle, &b, 1);

        /* Send the ACK/NAK Sequence Number */
        b = (uint8_t)(fcb->acknak & 0xFF);
        crc = CRC16Update(crc, b);
        UART_write(handle, &b, 1);

        /* Send the Text length (MSB) */
        b = (uint8_t)((textlen >> 8) & 0xFF);
        crc = CRC16Update(crc, b);
        UART_write(handle, &b, 1);

        /* Send the Text length (LSB) */
        b = (uint8_t)(textlen & 0xFF);
        crc = CRC16Update(crc, b);
        UART_write(handle, &b, 1);

        /* Send any text data associated with the frame */

        if (textbuf && textlen)
        {
            for (i=0; i < textlen; i++)
            {
                b = *textbuf++;
                crc = CRC16Update(crc, b);
                UART_write(handle, &b, 1);
            }
        }
    }

    /* Send the CRC MSB */
    b = (uint8_t)(crc >> 8);
    UART_write(handle, &b, 1);

    /* Send the CRC LSB */
    b = (uint8_t)(crc & 0xFF);
    UART_write(handle, &b, 1);

    return IPC_ERR_SUCCESS;
}

// End-Of-File
