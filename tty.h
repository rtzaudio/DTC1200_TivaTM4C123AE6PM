/*
 * tty.h : created 7/30/99 10:50:41 AM
 *  
 * Copyright (C) 2016, RTZ Professional Audio, LLC
 * ALL RIGHTS RESERVED
 *  
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION OF RTZ AUDIO. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF RTZ AUDIO.
 */

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
