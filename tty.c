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

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "DTC1200.h"
#include "PID.h"
#include "Globals.h"
#include "ServoTask.h"
#include "tty.h"

/*
 * TTY Terminal Support
 */

int tty_getc(int* pch)
{
	char cRxedChar;
	int nCount;

	nCount = UART_read(g_handleUartTTY, &cRxedChar, 1);

	if (nCount)
		*pch = (int)cRxedChar;
	else
		*pch = 0;

	return nCount;
}

void tty_putc(char c)
{
	UART_write(g_handleUartTTY, &c, sizeof(c));
} 

void tty_rxflush(void)
{
}

void tty_puts(const char* s)
{
	int len = strlen(s);
	UART_write(g_handleUartTTY, s, len);\
}

/* postion the cursor (row, col) */

void tty_pos(int row, int col)
{
	int n;
    static char buf[32];
    /* Position the cursor */
    n = sprintf(buf, VT100_POS, row, col);

    if (n >= 32)
    	System_abort("tty_pos() overflow!\n");

    tty_puts(buf);
}

void tty_printf(const char *fmt, ...)
{
    int n;
    va_list arg;
    static char buf[256];

    va_start(arg, fmt);
	n = vsprintf(buf, fmt, arg);
    va_end(arg);

    if (n >= 256)
    	System_abort("tty_printf() overflow!\n");

    UART_write(g_handleUartTTY, buf, strlen(buf));
}

void tty_cls(void)
{
    tty_puts(VT100_HOME);
    tty_puts(VT100_CLS);
}

void tty_aputs(int row, int col, char* s)
{
    tty_pos(row, col);
    tty_puts(s);
}

void tty_erase_line(void)
{
    tty_puts(VT100_ERASE_LINE);
}

void tty_erase_eol(void)
{
    tty_puts(VT100_ERASE_EOL);
}

/* end-of-file */
