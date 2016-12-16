/*
 * tty.c : created 7/30/99 10:50:41 AM
 *  
 * Copyright (C) 2016, RTZ Professional Audio, LLC.
 *
 * ALL RIGHTS RESERVED.
 *  
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION OF RTZ AUDIO. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF RTZ AUDIO.
 */

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
	UART_write( g_handleUartTTY, &c, sizeof(c) );
} 

void tty_rxflush(void)
{
}

void tty_cls(void)
{
    tty_puts(VT100_HOME);
    tty_puts(VT100_CLS);
}

void tty_puts(const char* s)
{
	UART_write(g_handleUartTTY, s, strlen(s));
}

void tty_aputs(int row, int col, char* s)
{
    tty_pos(row, col);
    tty_puts(s);
}

/* postion the cursor (row, col) */

void tty_pos(int row, int col)
{
    static char buf[32];
    /* Postion the cursor */
    sprintf(buf, VT100_POS, row, col);
    tty_puts(buf);
}

void tty_erase_line(void)
{
    tty_puts(VT100_ERASE_LINE);
}

void tty_erase_eol(void)
{
    tty_puts(VT100_ERASE_EOL);
}

void tty_printf(const char *fmt, ...)
{
    static char buf[256];

    va_list arg;

    va_start(arg, fmt);
	vsprintf(buf, fmt, arg);
    va_end(arg);

    UART_write(g_handleUartTTY, buf, strlen(buf));
}

/* end-of-file */
