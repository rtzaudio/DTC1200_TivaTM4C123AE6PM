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

/*
 *    ======== tcpEcho.c ========
 *    Contains BSD sockets code.
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>

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

/* PMX42 Board Header file */
#include "Board.h"
#include "RAMP.h"

/* Static Function Prototypes */

static uint16_t CRC16Update(uint16_t crc, uint8_t data);

//*****************************************************************************
// Initialize FCB structure to default values
//*****************************************************************************

void RAMP_InitFcb(FCB* fcb)
{
	fcb->type    = MAKETYPE(0, TYPE_MSG_ONLY);
	fcb->seqnum  = MIN_SEQ_NUM;
	fcb->acknak  = 0;
	fcb->address = 0;
}

//*****************************************************************************
// Transmit a RAMP frame of data out the RS-422 port
//*****************************************************************************

int RAMP_TxFrame(UART_Handle handle, FCB* fcb, void* text, uint16_t textlen)
{
	uint8_t b;
	uint8_t type;
    uint16_t i;
    uint16_t framelen;
    uint16_t crc = 0;
    uint8_t *textbuf = (uint8_t*)text;

	/* First check the text length is valid */
	if (textlen > MAX_TEXT_LEN)
		return ERR_TEXT_LEN;

    /* Get the frame type less any flag bits */
    type = (fcb->type & FRAME_TYPE_MASK);

    /* Are we sending a ACK or NAK only frame? */
    if ((type == TYPE_ACK_ONLY) || (type == TYPE_NAK_ONLY))
    {
        textbuf = NULL;
        textlen = 0;

        framelen = ACK_FRAME_LEN;

		/* Set the ACK/NAK flag bit */
		fcb->type |= F_ACKNAK;
    }
    else
    {
        framelen = textlen + (FRAME_OVERHEAD - PREAMBLE_OVERHEAD);

        /* If message is piggyback ACK/NAK, set flag bit also */
        if ((type == TYPE_MSG_ACK) || (type == TYPE_MSG_NAK))
            fcb->type |= F_ACKNAK;
        else
            fcb->type &= ~(F_ACKNAK);
    }

    /* Send the preamble MSB for the frame start */
    b = PREAMBLE_MSB;
    UART_write(handle, &b, 1);

    /* Send the preamble LSB for the frame start */
    b = PREAMBLE_LSB;
    UART_write(handle, &b, 1);

    /* CRC starts here, sum in the seed byte first */
    crc = CRC16Update(crc, CRC_SEED_BYTE);

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

    /* Send the frame address byte */
    b = (uint8_t)(fcb->address & 0xFF);
    crc = CRC16Update(crc, b);
    UART_write(handle, &b, 1);

    /* Sending ACK or NAK only frame? */

    if ((type == TYPE_ACK_ONLY) || (type == TYPE_NAK_ONLY))
    {
		/* Sending ACK/NAK frame only  */

		b = (uint8_t)(fcb->acknak & 0xFF);
		crc = CRC16Update(crc, b);
		UART_write(handle, &b, 1);
    }
    else
    {
    	/* Continue sending a full RAMP frame */

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

    return ERR_SUCCESS;
}

//*****************************************************************************
// Receive a RAMP data frame from the RS-422 port
//*****************************************************************************

int RAMP_RxFrame(UART_Handle handle, FCB* fcb, void* text, uint16_t textlen)
{
    int i;
	int rc = ERR_SUCCESS;
	uint8_t b;
    uint8_t type;
    uint16_t lsb;
    uint16_t msb;
    uint16_t framelen;
    uint16_t rxcrc;
    uint16_t crc = 0;
    uint8_t *textbuf = (uint8_t*)text;

    /* First, try to synchronize to 0x89 SOF byte */

    i = 0;

    do {

        /* Read the preamble MSB for the frame start */
        if (UART_read(handle, &b, 1) != 1)
            return ERR_TIMEOUT;

        /* Garbage flood check, synch lost?? */
        if (i++ > (FRAME_OVERHEAD + PREAMBLE_OVERHEAD + MAX_TEXT_LEN))
            return ERR_SYNC;

    } while (b != PREAMBLE_MSB);

    /* We found the first 0x89 sequence, next byte
     * has to be 0xFC for a valid SOF sequence.
     */

    if (UART_read(handle, &b, 1) != 1)
        return ERR_TIMEOUT;

    if (b != PREAMBLE_LSB)
        return ERR_SYNC;

    /* CRC starts here, sum in the seed byte first */
    crc = CRC16Update(crc, CRC_SEED_BYTE);

    /* Read the Frame length (MSB) */
    if (UART_read(handle, &b, 1) != 1)
    	return ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    msb = (uint16_t)b;

    /* Read the Frame length (LSB) */
    if (UART_read(handle, &b, 1) != 1)
    	return ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    lsb = (uint16_t)b;

    /* Build and validate maximum frame length */
    framelen = (size_t)((msb << 8) | lsb) & 0xFFFF;

    if (framelen > MAX_FRAME_LEN)
    	return ERR_FRAME_LEN;

    /* Read the Frame Type Byte */
    if (UART_read(handle, &b, 1) != 1)
    	return ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    fcb->type = b;

	/* Read the Frame Address Byte */
	if (UART_read(handle, &b, 1) != 1)
		return ERR_SHORT_FRAME;

	crc = CRC16Update(crc, b);
	fcb->address = b;

    /* Check for ACK/NAK only frame (type always 11H or 12H) */

	/* Get the frame type less any flag bits */
    type = (fcb->type & FRAME_TYPE_MASK);

    if ((framelen == ACK_FRAME_LEN) && ((type == TYPE_ACK_ONLY) || (type == TYPE_NAK_ONLY)))
    {
		/* Read the ACK/NAK Sequence Number */
		if (UART_read(handle, &b, 1) != 1)
			return ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		fcb->acknak = b;
    }
    else
    {
    	/* It's a full RAMP frame, continue decoding the rest of the frame */

		/* Read the Frame Sequence Number */
		if (UART_read(handle, &b, 1) != 1)
			return ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		fcb->seqnum = b;

		/* Read the ACK/NAK Sequence Number */
		if (UART_read(handle, &b, 1) != 1)
			return ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		fcb->acknak = b;

		/* Read the Text length (MSB) */
		if (UART_read(handle, &b, 1) != 1)
			return ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		msb = (uint16_t)b;

		/* Read the Text length (LSB) */
		if (UART_read(handle, &b, 1) != 1)
			return ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		lsb = (uint16_t)b;

		/* Get the frame length received and validate it */
		uint16_t rxtextlen = (size_t)((msb << 8) | lsb) & 0xFFFF;

		/* The text length should be the frame overhead minus the preamble overhead
		 * plus the text length specified in the received frame. If these don't match
		 * then we have either a packet data error or a malformed packet.
		 */
		if (rxtextlen + (FRAME_OVERHEAD - PREAMBLE_OVERHEAD) != framelen)
			return ERR_TEXT_LEN;

		/* Read text data associated with the frame */

		for (i=0; i < rxtextlen; i++)
		{
			if (UART_read(handle, &b, 1) != 1)
				return ERR_SHORT_FRAME;

			/* update the CRC */
			crc = CRC16Update(crc, b);

			/* If we overflow, continue reading the packet
			 * data, but don't store the data into the buffer.
			 */
			if (i >= textlen)
			{
				rc = ERR_RX_OVERFLOW;
				continue;
			}

			if (textbuf)
				*textbuf++ = b;
		}
    }

    /* Read the packet CRC MSB */
    if (UART_read(handle, &b, 1) != 1)
    	return ERR_SHORT_FRAME;

    msb = (uint16_t)b & 0xFF;

    /* Read the packet CRC LSB */
    if (UART_read(handle, &b, 1) != 1)
    	return ERR_SHORT_FRAME;

    lsb = (uint16_t)b & 0xFF;

    /* Build and validate the CRC */
    rxcrc = (uint16_t)((msb << 8) | lsb) & 0xFFFF;

    /* Validate the CRC values match */
    if (rxcrc != crc)
    	rc = ERR_CRC;

    return rc;
}

//*****************************************************************************
// Update the CRC-16 sum value for a byte
//*****************************************************************************

uint16_t CRC16Update(uint16_t crc, uint8_t data)
{
    int i;

    crc = crc ^ ((uint16_t)data << 8);

    for (i=0; i < 8; i++)
    {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }

    return crc;
}

// End-Of-File
