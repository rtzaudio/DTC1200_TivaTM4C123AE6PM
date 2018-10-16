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
#include <ti/drivers/UART.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Board Header file */
#include "Board.h"
#include "SIMP.h"

/* Static Function Prototypes */

static uint16_t CRC16Update(uint16_t crc, uint8_t data);

/* Static Data Items */

static SIMP_SERVICE g_simp;

//*****************************************************************************
// Allocate resources and initialize the SIMP Service.
//*****************************************************************************

Bool SIMP_Service_init(void)
{
    Int i;
    SIMP_FCB* fcb;
    SIMP_MSG* msg;
    Error_Block eb;
    UART_Params uartParams;

    /* Open the UART for binary mode */

    UART_Params_init(&uartParams);

    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 2000;                   // 2 second read timeout
    uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
    uartParams.readCallback   = NULL;
    uartParams.writeCallback  = NULL;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readEcho       = UART_ECHO_OFF;
    uartParams.baudRate       = 250000;
    uartParams.stopBits       = UART_STOP_ONE;
    uartParams.parityType     = UART_PAR_NONE;

    g_simp.uartHandle = UART_open(Board_UART_IPC, &uartParams);

    if (g_simp.uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /* Create the queues needed */
    g_simp.txFreeQue = Queue_create(NULL, NULL);
    g_simp.txDataQue = Queue_create(NULL, NULL);
    g_simp.rxFreeQue = Queue_create(NULL, NULL);
    g_simp.rxDataQue = Queue_create(NULL, NULL);

    /* Create semaphores needed */
    g_simp.txFreeSem = Semaphore_create(SIMP_MAX_WINDOW, NULL, NULL);
    g_simp.txDataSem = Semaphore_create(0, NULL, NULL);
    g_simp.rxFreeSem = Semaphore_create(SIMP_MAX_WINDOW, NULL, NULL);
    g_simp.rxDataSem = Semaphore_create(0, NULL, NULL);

    Error_init(&eb);
    g_simp.ackEvent  = Event_create(NULL, NULL);

    /*
     * Allocate and Initialize TRANSMIT Memory Buffers
     */

    Error_init(&eb);
    fcb = (SIMP_FCB*)Memory_alloc(NULL, sizeof(SIMP_FCB) * SIMP_MAX_WINDOW, 0, &eb);
    g_simp.txFCB = fcb;
    if (fcb == NULL)
        System_abort("Tx FCB alloc failed");

    Error_init(&eb);
    msg = (SIMP_MSG*)Memory_alloc(NULL, sizeof(SIMP_MSG) * SIMP_MAX_WINDOW, 0, &eb);
    g_simp.txMsg = msg;
    if (msg == NULL)
        System_abort("Tx MSG alloc failed");

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < SIMP_MAX_WINDOW; i++, fcb++, msg++)
    {
        /* Set FCB to point to tx message buffer */
        fcb->textbuf = msg;
        fcb->textlen = sizeof(SIMP_MSG);

        /* Place FCB on the free queue list */
        Queue_enqueue(g_simp.txFreeQue, (Queue_Elem*)fcb);
    }

    /*
     * Allocate and Initialize RECEIVE Memory Buffers
     */

    Error_init(&eb);
    fcb = (SIMP_FCB*)Memory_alloc(NULL, sizeof(SIMP_FCB) * SIMP_MAX_WINDOW, 0, &eb);
    g_simp.rxFCB = fcb;
    if (fcb == NULL)
        System_abort("Rx FCB alloc failed");

    Error_init(&eb);
    msg = (SIMP_MSG*)Memory_alloc(NULL, sizeof(SIMP_MSG) * SIMP_MAX_WINDOW, 0, &eb);
    g_simp.rxMsg = msg;
    if (msg == NULL)
        System_abort("Tx MSG alloc failed");

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < SIMP_MAX_WINDOW; i++, fcb++, msg++)
    {
        /* Set FCB to point to rx message buffer */
        fcb->textbuf = msg;
        fcb->textlen = sizeof(SIMP_MSG);

        /* Place FCB on the rx free queue list */
        Queue_enqueue(g_simp.rxFreeQue, (Queue_Elem*)fcb);
    }

    /* Initialize Server Data Items */

    g_simp.txErrors      = 0;
    g_simp.txCount       = 0;
    g_simp.txNumFreeMsgs = SIMP_MAX_WINDOW;
    g_simp.txNextSeq     = SIMP_MIN_SEQ;    /* current tx sequence# */

    g_simp.rxErrors      = 0;
    g_simp.rxCount       = 0;
    g_simp.rxNumFreeMsgs = SIMP_MAX_WINDOW;
    g_simp.rxLastSeq     = 0;               /* last seq# accepted   */
    g_simp.rxExpectedSeq = SIMP_MIN_SEQ;    /* expected recv seq#   */

    return TRUE;
}

//*****************************************************************************
// This function blocks until an IPC message is available in the rx queue or
// the timeout expires. A return FALSE value indicates the timeout expired
// or a buffer never became available for the receiver within the timeout
// period specified.
//*****************************************************************************

Bool SIMP_Message_pend(SIMP_MSG* msg, SIMP_FCB* txFcb, UInt32 timeout)
{
    UInt key;
    SIMP_FCB* elem;

    if (Semaphore_pend(g_simp.rxDataSem, timeout))
    {
        /* get message from dataQue */
        elem = Queue_get(g_simp.rxDataQue);

        /* perform the enqueue and increment numFreeMsgs atomically */
        key = Hwi_disable();

        /* put message on freeQue */
        Queue_enqueue(g_simp.rxFreeQue, (Queue_Elem *)elem);

        /* increment numFreeMsgs */
        g_simp.rxNumFreeMsgs++;

        /* re-enable ints */
        Hwi_restore(key);

        /* return message and fcb data to caller */
        //memcpy(msg, &(elem->msg), sizeof(SIMP_MSG));
        //memcpy(fcb, &(elem->fcb), sizeof(RAMP_FCB));

        /* post the semaphore */
        Semaphore_post(g_simp.rxFreeSem);

        return TRUE;
    }

    return FALSE;
}

//*****************************************************************************
// This function posts a message to the transmit queue. A return FALSE value
// indicates the timeout expired or a buffer never became available for
// transmission within the timeout period specified.
//*****************************************************************************

Bool SIMP_Message_post(SIMP_MSG* msg, SIMP_FCB* fcb, UInt32 timeout)
{
    UInt key;
    SIMP_FCB* elem;

    /* Wait for a free transmit buffer and timeout if necessary */
    if (Semaphore_pend(g_simp.txFreeSem, timeout))
    {
        /* perform the dequeue and decrement numFreeMsgs atomically */
        key = Hwi_disable();

        /* get a message from the free queue */
        elem = Queue_dequeue(g_simp.txFreeQue);

        /* Make sure that a valid pointer was returned. */
        if (elem == (SIMP_FCB*)(g_simp.txFreeQue))
        {
            Hwi_restore(key);
            return FALSE;
        }

        /* decrement the numFreeMsgs */
        g_simp.txNumFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* copy msg to element */
        //memcpy(&(elem->fcb), fcb, sizeof(RAMP_FCB));
        //memcpy(&(elem->msg), msg, sizeof(SIMP_MSG));

        /* put message on txDataQueue */
        if (fcb->type & SIMP_F_PRIORITY)
            Queue_putHead(g_simp.txDataQue, (Queue_Elem *)elem);
        else
            Queue_put(g_simp.txDataQue, (Queue_Elem *)elem);

        /* post the semaphore */
        Semaphore_post(g_simp.txDataSem);

        return TRUE;          /* success */
    }

    return FALSE;         /* error */
}

//*****************************************************************************
// LOW LEVEL FRAME TX/RX FUNCTIONS
//*****************************************************************************

void SIMP_SetTextBuf(SIMP_FCB* fcb, void *textbuf, size_t textlen)
{
	fcb->textbuf = textbuf;
	fcb->textlen = textlen;
}

//*****************************************************************************
// This function returns the next available transmit and increments the
// counter to the next frame sequence number.
//*****************************************************************************

uint8_t SIMP_GetTxSeqNum(void)
{
    /* increment sequence number atomically */
    UInt key = Hwi_disable();

    /* Get the next frame sequence number */
    uint8_t seqnum = g_simp.txNextSeq;

    /* Increment the servers sequence number */
    g_simp.txNextSeq = SIMP_INC_SEQ(seqnum);

    /* re-enable ints */
    Hwi_restore(key);

    return seqnum;
}

//*****************************************************************************
// Transmit a SIMP frame of data out the RS-422 port
//*****************************************************************************

int SIMP_TxFrame(UART_Handle handle, SIMP_FCB* fcb)
{
	uint8_t b;
	uint8_t type;
    uint16_t i;
    uint16_t framelen;
    uint16_t crc;

    uint8_t *textbuf = (uint8_t*)fcb->textbuf;
    uint16_t textlen = (uint16_t)fcb->textlen;

	/* First check the text length is valid */
	if (textlen > SIMP_MAX_TEXT_LEN)
		return SIMP_ERR_TEXT_LEN;

    /* Get the frame type less any flag bits */
    type = (fcb->type & SIMP_TYPE_MASK);

    /* Are we sending a ACK or NAK only frame? */
    if ((type == SIMP_ACK_ONLY) || (type == SIMP_NAK_ONLY))
    {
        textbuf = NULL;
        textlen = 0;

        framelen = SIMP_ACK_FRAME_LEN;

		/* Set the ACK/NAK flag bit */
		fcb->type |= SIMP_F_ACKNAK;
    }
    else
    {
        /* Build the frame length with text length given */
        framelen = textlen + (SIMP_FRAME_OVERHEAD - SIMP_PREAMBLE_OVERHEAD);

        /* If message is piggyback ACK/NAK, set flag bit also */
        if ((type == SIMP_MSG_ACK) || (type == SIMP_MSG_NAK))
            fcb->type |= SIMP_F_ACKNAK;
        else
            fcb->type &= ~(SIMP_F_ACKNAK);
    }

    /* Send the preamble MSB for the frame start */
    b = SIMP_PREAMBLE_MSB;
    UART_write(handle, &b, 1);

    /* Send the preamble LSB for the frame start */
    b = SIMP_PREAMBLE_LSB;
    UART_write(handle, &b, 1);

    /* CRC starts here, sum in the seed byte first */
    crc = CRC16Update(0, SIMP_CRC_SEED_BYTE);

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

    if ((type == SIMP_ACK_ONLY) || (type == SIMP_NAK_ONLY))
    {
		/* Sending ACK/NAK frame only  */

		b = (uint8_t)(fcb->acknak & 0xFF);
		crc = CRC16Update(crc, b);
		UART_write(handle, &b, 1);
    }
    else
    {
    	/* Continue sending a full SIMP frame */

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

    return SIMP_ERR_SUCCESS;
}

//*****************************************************************************
// Receive a SIMP data frame from the RS-422 port
//*****************************************************************************

int SIMP_RxFrame(UART_Handle handle, SIMP_FCB* fcb)
{
    int i;
	int rc = SIMP_ERR_SUCCESS;
	uint8_t b;
    uint8_t type;
    uint16_t lsb;
    uint16_t msb;
    uint16_t framelen;
    uint16_t rxcrc;
    uint16_t crc = 0;

    uint8_t *textbuf = (uint8_t*)fcb->textbuf;
    uint16_t textlen = (uint16_t)fcb->textlen;

    /* First, try to synchronize to 0x79 SOF byte */

    fcb->state = 0;

    i = 0;

    do {

        /* Read the preamble MSB for the frame start */
        if (UART_read(handle, &b, 1) != 1)
            return SIMP_ERR_TIMEOUT;

        /* Garbage flood check, synch lost?? */
        if (i++ > (SIMP_FRAME_OVERHEAD + SIMP_PREAMBLE_OVERHEAD + SIMP_MAX_TEXT_LEN))
            return SIMP_ERR_SYNC;

    } while (b != SIMP_PREAMBLE_MSB);

    /* We found the first 0x79 sequence, next byte
     * has to be 0xFC for a valid SOF sequence.
     */

    if (UART_read(handle, &b, 1) != 1)
        return SIMP_ERR_TIMEOUT;

    if (b != SIMP_PREAMBLE_LSB)
        return SIMP_ERR_SYNC;

    /* Got the preamble word */
    fcb->state = 1;

    /* CRC starts here, sum in the seed byte first */
    crc = CRC16Update(crc, SIMP_CRC_SEED_BYTE);

    /* Read the Frame length (MSB) */
    if (UART_read(handle, &b, 1) != 1)
    	return SIMP_ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    msb = (uint16_t)b;

    /* Read the Frame length (LSB) */
    if (UART_read(handle, &b, 1) != 1)
    	return SIMP_ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    lsb = (uint16_t)b;

    /* Build and validate maximum frame length */
    framelen = (size_t)((msb << 8) | lsb) & 0xFFFF;

    if (framelen > SIMP_MAX_FRAME_LEN)
    	return SIMP_ERR_FRAME_LEN;

    /* Got frame length word */
    fcb->state = 2;

    /* Read the Frame Type Byte */
    if (UART_read(handle, &b, 1) != 1)
    	return SIMP_ERR_SHORT_FRAME;

    crc = CRC16Update(crc, b);
    fcb->type = b;

    /* Got the frame type byte */
    fcb->state = 3;

    /* Check for ACK/NAK only frame (type always 11H or 12H) */

	/* Get the frame type less any flag bits */
    type = (fcb->type & SIMP_TYPE_MASK);

    if ((type == SIMP_ACK_ONLY) || (type == SIMP_NAK_ONLY))
    {
        /* If ACK/NAK only, length must be ACK_FRAME_LEN */
        if (framelen != SIMP_ACK_FRAME_LEN)
            return SIMP_ERR_ACK_LEN;

		/* Read the ACK/NAK Sequence Number */
		if (UART_read(handle, &b, 1) != 1)
			return SIMP_ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		fcb->acknak = b;

	    /* Got ACK only frame */
	    fcb->state = 4;
    }
    else
    {
    	/* It's a full SIMP frame, continue decoding the rest of the frame */

		/* Read the Frame Sequence Number */
		if (UART_read(handle, &b, 1) != 1)
			return SIMP_ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		fcb->seqnum = b;

		/* Got the frame sequence number */
        fcb->state = 5;

		/* Read the ACK/NAK Sequence Number */
		if (UART_read(handle, &b, 1) != 1)
			return SIMP_ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		fcb->acknak = b;

		/* Got the ack/nak sequence number */
        fcb->state = 6;

		/* Read the Text length (MSB) */
		if (UART_read(handle, &b, 1) != 1)
			return SIMP_ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		msb = (uint16_t)b;

		/* Read the Text length (LSB) */
		if (UART_read(handle, &b, 1) != 1)
			return SIMP_ERR_SHORT_FRAME;

		crc = CRC16Update(crc, b);
		lsb = (uint16_t)b;

		/* Get the frame length received and validate it */
		uint16_t rxtextlen = (size_t)((msb << 8) | lsb) & 0xFFFF;

		/* The text length should be the frame overhead minus the preamble overhead
		 * plus the text length specified in the received frame. If these don't match
		 * then we have either a packet data error or a malformed packet.
		 */
		if (rxtextlen + (SIMP_FRAME_OVERHEAD - SIMP_PREAMBLE_OVERHEAD) != framelen)
			return SIMP_ERR_TEXT_LEN;

		/* Got the frame text length word */
        fcb->state = 6;

		/* Read text data associated with the frame */

		for (i=0; i < rxtextlen; i++)
		{
			if (UART_read(handle, &b, 1) != 1)
				return SIMP_ERR_SHORT_FRAME;

			/* update the CRC */
			crc = CRC16Update(crc, b);

			/* If we overflow, continue reading the packet
			 * data, but don't store the data into the buffer.
			 */
			if (i >= textlen)
			{
				rc = SIMP_ERR_RX_OVERFLOW;
				continue;
			}

			if (textbuf)
				*textbuf++ = b;
		}

		/* Got the frame text data */
        fcb->state = 7;
    }

    /* Read the packet CRC MSB */
    if (UART_read(handle, &b, 1) != 1)
    	return SIMP_ERR_SHORT_FRAME;

    msb = (uint16_t)b & 0xFF;

    /* Read the packet CRC LSB */
    if (UART_read(handle, &b, 1) != 1)
    	return SIMP_ERR_SHORT_FRAME;

    lsb = (uint16_t)b & 0xFF;

    /* Build and validate the CRC */
    rxcrc = (uint16_t)((msb << 8) | lsb) & 0xFFFF;

    /* Validate the CRC values match */
    if (rxcrc != crc)
    	rc = SIMP_ERR_CRC;

    /* Got valid CRC word */
    fcb->state = 8;

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
