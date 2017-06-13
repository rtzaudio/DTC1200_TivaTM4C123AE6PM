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

#ifndef DTC1200_TIVATM4C123AE6PMI_IOEXPANDER_H_
#define DTC1200_TIVATM4C123AE6PMI_IOEXPANDER_H_

typedef struct IOExpander_InitData {
    uint8_t	addr;
    uint8_t data;
} IOExpander_InitData;

typedef struct IOExpander_Object {
    SPI_Handle      		spiHandle;		/* Handle for SPI object */
    uint32_t    			boardSPI; 		/* Board SPI in Board.h */
    uint32_t    			boardCS;  		/* Board chip select in Board.h */
    IOExpander_InitData* 	initData;
    uint32_t				initDataCount;
} IOExpander_Object;

typedef IOExpander_Object *IOExpander_Handle;

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

void IOExpander_initialize(void);

bool MCP23S17_write(
	IOExpander_Handle	handle,
    uint8_t   			ucRegAddr,
    uint8_t   			ucData
    );

bool MCP23S17_read(
	IOExpander_Handle	handle,
    uint8_t				ucRegAddr,
    uint8_t*			pucData
    );

/* These functions access the I/O Expanders */

uint32_t GetInterruptFlags(uint8_t* pucIntFlags, uint8_t* pucCapFlags);

uint32_t GetTransportSwitches(uint8_t* pucMask);
uint32_t GetModeSwitches(uint8_t* pucMask);

uint32_t SetTransportMask(uint8_t ucSetMask, uint8_t ucClearMask);
uint8_t GetTransportMask(void);

uint32_t SetLamp(uint8_t ucBitMask);
uint32_t SetLampMask(uint8_t ucSetMask, uint8_t ucClearMask);
uint8_t GetLampMask(void);

#endif /*DTC1200_TIVATM4C123AE6PMI_IOEXPANDER_H_*/
