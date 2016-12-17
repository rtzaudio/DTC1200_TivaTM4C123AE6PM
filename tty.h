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

/* VT100 Escape Sequences */
#define VT100_HOME          "\x01b[1;1;H"       /* home cursor */
#define VT100_CLS           "\x01b[2J"          /* clear screen, home cursor */
#define VT100_BELL          "\x07"              /* bell */
#define VT100_POS           "\x01b[%d;%d;H"     /* pstn cursor (row, col) */
#define VT100_UL_ON         "\x01b#7\x01b[4m"   /* underline on */
#define VT100_UL_OFF        "\x01b[0m"          /* underline off */
#define VT100_INV_ON        "\x01b#7\x01b[7m"   /* inverse on */
#define VT100_INV_OFF       "\x01b[0m"          /* inverse off */
#define VT100_ERASE_EOL     "\x01b[K"           /* erase to end of line */
#define VT100_ERASE_SOL     "\x01b[1K"          /* erase to start of line */
#define VT100_ERASE_LINE    "\x01b[2K"          /* erase entire line */

/* ASCII Codes */
#define SOH     0x01
#define STX     0x02
#define ENQ     0x05
#define ACK     0x06
#define BELL    0x07
#define BKSPC   0x08
#define LF      0x0A
#define CRET    0x0D
#define NAK     0x15
#define SYN     0x16
#define ESC     0x1B
#define CTL_X   0x18

 /* tty.c */
void tty_cls(void);
void tty_aputs(int row, int col, char* s);
void tty_erase_line(void);
void tty_erase_eol(void);
void tty_pos(int row, int col);
void tty_printf(const char* fmt, ...);
void tty_rxflush(void);
void tty_keywait(void);
void tty_rxflush(void);
void tty_putc(char c);
void tty_puts(const char* s);
int tty_getc(int* pch);

/* end-of-file */
