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

#ifndef __BOARD_H
#define __BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "DTC1200_TivaTM4C123AE6PMI.h"

#define Board_initGeneral           DTC1200_initGeneral
#define Board_initGPIO              DTC1200_initGPIO
#define Board_initI2C               DTC1200_initI2C
#define Board_initSPI               DTC1200_initSPI
#define Board_initUART              DTC1200_initUART
#define Board_initQEI				DTC1200_initQEI
#define Board_initADC				DTC1200_initADC
#define Board_initWatchdog          DTC1200_initWatchdog

#define Board_readADC				DTC1200_readADC

#define Board_I2C0					DTC1200_I2C0		// I2C0 : CAT24C08 EPROM
#define Board_I2C1					DTC1200_I2C1		// I2C1 : AT24CS01 SERIAL#/EPROM

#define Board_SPI0                  DTC1200_SPI0		// SSI-0 : SPI SUPPLY & TAKEUP SERVO DAC
#define Board_SPI1                  DTC1200_SPI1		// SSI-1 : SPI MCP23S17SO TRANSPORT SWITCHES & LAMPS
#define Board_SPI2                  DTC1200_SPI2		// SSI-2 : SPI MCP23S17SO SOLENOID, CONFIG DIP SWITCH & TAPE OUT

#define Board_UART_IPC              DTC1200_UART0		// RS-232 IPC on edge connector to timer/counter board
#define Board_UART_TTY              DTC1200_UART1		// RS-232 for TTY terminal console
#define Board_UART_EXP              DTC1200_UART5		// RS-232 on expansion port connector P1

#define Board_WATCHDOG0             DTC1200_WATCHDOG0

#define Board_LED_ON                DTC1200_LED_ON
#define Board_LED_OFF               DTC1200_LED_OFF

/* GPIO Pin Mappings */
#define Board_CS_SPI0				DTC1200_SSI0FSS				/* PA3 */
#define Board_CS_SPI2				DTC1200_SSI2FSS				/* PB5 */
#define Board_TAPE_END				DTC1200_TAPE_END			/* PC7 */
#define Board_EXPANSION_PC6			DTC1200_EXPANSION_PC6		/* PC6 */
#define Board_RESET_MCP				DTC1200_RESET_MCP			/* PD4 */
#define Board_EXPANSION_PD5			DTC1200_EXPANSION_PD5		/* PD5 */
#define Board_CS_SPI1				DTC1200_SSI1FSS				/* PD1 */
#define Board_EXPANSION_PF3			DTC1200_EXPANSION_PF3		/* PF3 */
#define Board_EXPANSION_PF2			DTC1200_EXPANSION_PF2		/* PF2 */
#define Board_INT1A					DTC1200_MCP23S17T_INT1A		/* PG4 */
#define Board_INT2B					DTC1200_MCP23S17T_INT2B		/* PG3 */
#define Board_TAPE_WIDTH			DTC1200_TAPE_WIDTH			/* PG2 */

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_H */
