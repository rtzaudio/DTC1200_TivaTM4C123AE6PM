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

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* Tivaware Driver files */
#include <driverlib/eeprom.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//#include <driverlib/sysctl.h>

/* XDCtools Header files */
#include "Board.h"
#include "DTC1200.h"
#include "Globals.h"

static SPI_Handle g_handleSpi0 = 0;

extern Semaphore_Handle g_semaSPI;

//*****************************************************************************
// Initialize and open various system peripherals we'll be using
//*****************************************************************************

void MotorDAC_initialize(void)
{
	SPI_Params spiParams;

	/* Deassert the DAC chip select */
	GPIO_write(Board_CS_SPI0, PIN_HIGH);

	/* Open SSI-0 to the TLV5637 DAC motor drive amp driver */

	/* Moto fmt, polarity 1, phase 0 */
    SPI_Params_init(&spiParams);

    spiParams.transferMode	= SPI_MODE_BLOCKING;
    spiParams.mode 			= SPI_MASTER;
    spiParams.frameFormat 	= SPI_POL1_PHA0;
    spiParams.bitRate 		= 1000000;
	spiParams.dataSize 		= 16;
	spiParams.transferCallbackFxn = NULL;

	g_handleSpi0 = SPI_open(Board_SPI0, &spiParams);

	if (g_handleSpi0 == NULL)
	    System_abort("Error initializing SPI0\n");

	/* Set the torque to zero on reel motors! */
	MotorDAC_write(0, 0);
}

//*****************************************************************************
// This function writes the takeup and supply motor DAC values controlling
// the motor drive amp. The TLV5637 is a dual 10-bit, single supply DAC,
// based on a resistor string architecture. The output voltage (full scale
// determined by reference) is given by:
//
//      Vout = (2 REF) * (CODE/0x1000)
//
// Where REF is the reference voltage and CODE is the digital
// input value in the range 0x000 to 0xFFF. Because it is a
// 10-bit DAC, only D11 to D2 are used. D0 and D1 are ignored.
// A power-on reset initially puts the internal latches to a
// defined state (all bits zero).
//
// The motor current amp delivers full torque at 1mA and
// zero torque at 5.1mA.
//
//      DAC A - is the SUPPLY motor torque level
//      DAC B - is the TAKEUP motor torque level
//
//*****************************************************************************

void MotorDAC_write(uint32_t supply, uint32_t takeup)
{
	uint16_t ulWord;
	uint16_t ulReply;
	uint16_t ulDac;
	bool transferOK;
	SPI_Transaction transaction;

    //if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
		/* DEBUG - save current values */
		g_servo.dac_supply = supply;
		g_servo.dac_takeup = takeup;

		takeup = DAC_MAX - takeup;
		supply = DAC_MAX - supply;

		/* (1) Set reference voltage to 1.024 V (CONTROL register) */

		ulWord = (1 << 15) | (1 << 12) | 0x01;

		transaction.count = 1;	//sizeof(ulWord);
		transaction.txBuf = (Ptr)&ulWord;
		transaction.rxBuf = (Ptr)&ulReply;

		GPIO_write(Board_CS_SPI0, PIN_LOW);
		transferOK = SPI_transfer(g_handleSpi0, &transaction);
		GPIO_write(Board_CS_SPI0, PIN_HIGH);

		/* (2) Write data for DAC B to BUFFER */

		ulDac  = (takeup & 0x3FF) << 2;
		ulWord = (1 << 12) | (uint16_t)ulDac;

		transaction.count = 1;	//sizeof(ulWord);
		transaction.txBuf = (Ptr)&ulWord;
		transaction.rxBuf = (Ptr)&ulReply;

		GPIO_write(Board_CS_SPI0, PIN_LOW);
		transferOK = SPI_transfer(g_handleSpi0, &transaction);
		GPIO_write(Board_CS_SPI0, PIN_HIGH);

		/* (3) Write DAC A value and update DAC A & B simultaneously */

		ulDac  = (supply & 0x3FF) << 2;
		ulWord = (1 << 15) | (uint16_t)ulDac;

		transaction.count = 1;	//sizeof(ulWord);
		transaction.txBuf = (Ptr)&ulWord;
		transaction.rxBuf = (Ptr)&ulReply;

		GPIO_write(Board_CS_SPI0, PIN_LOW);
		transferOK = SPI_transfer(g_handleSpi0, &transaction);
		GPIO_write(Board_CS_SPI0, PIN_HIGH);

		//Semaphore_post(g_semaSPI);
    }

    if (!transferOK)
    	System_printf("DAC SPI write failed!\n");
}
