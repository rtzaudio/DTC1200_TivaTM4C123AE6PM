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
/* Project specific includes */
#include "DTC1200.h"
#include "PID.h"
#include "Globals.h"
#include "ServoTask.h"
#include "TerminalTask.h"
#include "Diag.h"
#include "tty.h"

/*****************************************************************************
 * TTY MENU SYSTEM CONSTANTS
 *****************************************************************************/

#define KEY_RET             LF      /* line feed */
#define KEY_ESC             ESC     /* ESC key   */

/* Edit mode states */
#define ES_MENU_SELECT      0       /* menu choice input selection state    */
#define ES_DATA_INPUT       1       /* numeric input state (range checked)  */
#define ES_STRING_INPUT     2       /* string input state (length checked)  */
#define ES_BITBOOL_SELECT   3       /* select on/off bitflag option state   */
#define ES_BITLIST_SELECT   4       /* select value from a list of bitflags */
#define ES_VALLIST_SELECT   5       /* select value from a discrete list    */

/* Arg types for prompt_menu_item() */
#define NEXT                1
#define PREV                2

/*****************************************************************************
 * STATIC DATA ITEMS
 *****************************************************************************/

/* keystroke input buffer */
static char s_keybuf[KEYBUF_SIZE+1];

static int s_keycount   = 0;        /* input key buffer count */
static int s_menu_num   = 0;        /* active menu number     */
static int s_edit_state = 0;        /* current edit state     */
static int s_max_chars  = 0;
static int s_end_debug  = 0;

static MENUITEM* s_menuitem = NULL; /* current menu item ptr  */
static long s_bitbool = 0;

static MENU_ARGLIST* s_bitlist = NULL;
static MENU_ARGLIST* s_vallist = NULL;

/* VT100 Terminal Attributes */

static const char s_ul_on[]   = VT100_UL_ON;
static const char s_ul_off[]  = VT100_UL_OFF;
static const char s_inv_on[]  = VT100_INV_ON;
static const char s_inv_off[] = VT100_INV_OFF;

static const char* s_escstr = "<ESC> or 'X' to exit...";
static const char* s_title  = "DTC-1200 Transport Controller v%u.%-2.2u";

/*****************************************************************************
 * STATIC FUNCTION PROTOTYPES
 *****************************************************************************/

static void show_home_menu(void);
static void show_menu(void);
static int do_menu_keystate(int key);
static int do_hot_key(MENU *menu, int ch);
static int is_valid_key(int ch, int fmt);
static int is_escape(int ch);
static void change_menu(long n);
static void do_backspace(void);
static void do_bufkey(int ch);
static void show_monitor_screen();
static void show_monitor_data();

static MENU* get_menu(void);
static MENUITEM* search_menu_item(MENU* menu, char *optstr);
static int prompt_menu_item(MENUITEM* item, int nextprev);
static long get_item_data(MENUITEM* item);
static void set_item_data(MENUITEM* item, long data);
static void set_item_text(MENUITEM* item, char *text);
static MENU_ARGLIST* find_bitlist_item(MENUITEM* item, long value);
static MENU_ARGLIST* find_vallist_item(MENUITEM* item, long value);
static int get_hex_str(char* ptext, uint8_t* pdata, int len);

/*****************************************************************************
 * EXTERNAL MENU DATA REFERENCE TABLE
 *****************************************************************************/

extern MENU menu_main;
extern MENU menu_diag;
extern MENU menu_general;
extern MENU menu_tension;
extern MENU menu_stop;
extern MENU menu_shuttle;
extern MENU menu_play;

/* MUST BE IN ORDER OF MENU_XX ID DEFINES! */

static MENU* menu_tab[] = {
    &menu_main,
    &menu_diag,
    &menu_general,
    &menu_tension,
    &menu_stop,
    &menu_shuttle,
    &menu_play
};

#define NUM_MENUS   (sizeof(menu_tab) / sizeof(MENU*))

/*****************************************************************************
 * TTY TERMINAL TASK
 *
 * This task drives the RS-232 serial VT100 terminal port. All system
 * configuration options are adjusted through the terminal port.
 *
 *****************************************************************************/

Void TerminalTask(UArg a0, UArg a1)
{
    Terminal_initialize();

    int ch;

    /* Show the home menu screen initially. */
    show_home_menu();

    for( ;; )
    {
        if (tty_getc(&ch) < 1)
        {
            if (g_sys.debug)
                show_monitor_data();
            continue;
        }

        /* read the keystroke */
        ch = toupper(ch);

        /* if debug mode was enabled and ESC pressed, disable it */
        if (s_end_debug)
        {
            s_end_debug = 0;
            s_edit_state = s_keycount = 0;
            show_menu();
            continue;
        }

        if (g_sys.debug)
        {
            if (is_escape(ch))
            {
                if (g_sys.debug == 1)
                {
                    g_sys.debug = 0;
                    s_edit_state = s_keycount = 0;
                    s_end_debug = 0;
                    show_menu();
                }
                else
                {
                    tty_printf("\r\nAny key continues...");
                    g_sys.debug = 0;
                    s_edit_state = s_keycount = 0;
                    s_end_debug = 1;
                }
            }
            continue;
        }

        do_menu_keystate(ch);
    }
}

/*
 * Initialize the RS-232 port for TTY operation.
 */

void Terminal_initialize(void)
{
    UART_Params uartParams;

    /* 57600, 38400, 19200, 9600, etc */
    uint32_t baud = (g_dip_switch & M_DIPSW1) ? 9600: 19200;

    /* Open the UART port for the TTY console */

    UART_Params_init(&uartParams);

    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 500;                    // 0.5 second read timeout
    uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
    uartParams.readCallback   = NULL;
    uartParams.writeCallback  = NULL;
    uartParams.readReturnMode = UART_RETURN_NEWLINE;
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_TEXT;
    uartParams.readEcho       = UART_ECHO_OFF;
    uartParams.baudRate       = baud;
    uartParams.dataLength     = UART_LEN_8;
    uartParams.parityType     = UART_PAR_NONE;
    uartParams.stopBits       = UART_STOP_ONE;

    g_handleUartTTY = UART_open(Board_UART_TTY, &uartParams);

    if (g_handleUartTTY == NULL)
        System_abort("Error initializing TTY UART\n");
}

/*****************************************************************************
 * EXPOSED MENU HANDLER FUNCTIONS
 *****************************************************************************/

/*
 * Reset and display the menu in default home menu state.
 */

void show_home_menu(void)
{
    s_menu_num = MENU_MAIN;
    s_keycount = 0;
    s_edit_state = ES_MENU_SELECT;
    s_end_debug = 0;

    show_menu();
}

/*
 * Clear the screen and display the menu that is currently active.
 */

void show_menu(void)
{
    int i;
    MENU_ARGLIST* pal;
    MENU* menu = get_menu();

    /* Clear the screen and show title */
    tty_cls();
    tty_pos(1, 2);
    tty_printf(s_title, FIRMWARE_VER, FIRMWARE_REV);

    /* Show the tape speed if main menu */

    if (menu->id == MENU_MAIN)
    {
        int speed = g_high_speed_flag ? 30 : 15;
        tty_pos(3, 69);
        tty_printf("%d IPS %d\"", speed, g_tape_width);
    }

    if (menu->id == MENU_GENERAL)
    {
        char buf[64];
        get_hex_str(buf, g_ui8SerialNumber, 16);
        tty_pos(18, 2);
        tty_printf("Serial# %s", buf);
    }

    /* Add the menu heading to top of screen */

    tty_pos(2, 78 - strlen(menu->heading));

    tty_printf(s_inv_on);
    tty_printf(menu->heading);
    tty_printf(s_inv_off);

    /* Add the menu items text */

    int count = menu->count;

    MENUITEM* item = menu->items;

    for (i = 0; i < count; i++, item++)
    {
        long data = 0;
        float fval = 0.0f;

        /* Preload any menu item data if it exists */

        if (item->datatype)
        {
            if (item->datatype != DT_FLOAT)
                data = get_item_data(item);
        }

        int row    = item->row;
        int col    = item->col;
        int type   = item->menutype;
        int param1 = item->param1.U;

        tty_pos(row, col);

        switch (type)
        {
        case MI_TEXT:
            if (param1)
                /* display with underline */
                tty_printf("%s%s%s", s_ul_on, item->text, s_ul_off);
            else
                /* display with normal */
                tty_printf("%s", item->text);
            break;

        case MI_EXEC:
        case MI_HOTKEY:
            if (param1)
                tty_printf("%s", item->text);
            else
                tty_printf("%-2s) %s", item->optstr, item->text);
            break;

        case MI_PROMPT:
            if (!strlen(item->text))
            {
                tty_printf("Option");

                /* append "esc exits" if not home menu */
                if (menu->id)
                    tty_printf(" (ESC or 'X' to exit)");

                tty_printf(": ");
            }
            else
                tty_printf("%s", item->text);
            break;

        case MI_NMENU:
            if (strlen(item->optstr))
                tty_printf("%-2s) %s", item->optstr, item->text);
            else
                tty_printf("%s", item->text);
            break;

        case MI_BITFLAG:
            tty_printf("%-2s) %s : ", item->optstr, item->text);
            if (data & item->param1.U)
                tty_printf("ON");
            else
                tty_printf("OFF");
            break;

        case MI_NUMERIC:
        	if (item->datatype == DT_FLOAT)
        	{
        		fval = (float)(*((float*) item->data));
        		tty_printf("%-2s) %s : %.2f", item->optstr, item->text, fval);
        	}
        	else
        	{
        		tty_printf("%-2s) %s : %u", item->optstr, item->text, data);
        	}
            break;

        case MI_BITLIST:
            tty_printf("%-2s) %s : ", item->optstr, item->text);
            if ((pal = find_bitlist_item(item, data)) != NULL)
                tty_printf(pal->text);
            break;

        case MI_VALLIST:
            tty_printf("%-2s) %s : ", item->optstr, item->text);
            if ((pal = find_vallist_item(item, data)) != NULL)
                tty_printf(pal->text);
            break;

        case MI_STRING:
            tty_printf("%-2s) %s : %s", item->optstr, item->text,
                    (char*)(item->data));
            break;
        }
    }
}

/*
 * Process a keystroke from the tty terminal input in single state mode.
 * Once the enter key is pressed, we process the menu item accordingly.
 */

int do_menu_keystate(int key)
{
    int res = 0;

    int ch = toupper(key);

    switch (s_edit_state)
    {
    case ES_MENU_SELECT:

        if (do_hot_key(get_menu(), ch))
        {
            /* reset menu state and key count */
            s_edit_state = ES_MENU_SELECT;
            s_keycount = 0;
            show_menu();
            break;
        }

        if (ch == BKSPC)
        {
            do_backspace();
        }
        else if (is_escape(ch) && (s_menu_num != MENU_MAIN))
        {
            change_menu(MENU_MAIN);
            show_menu();
        }
        else if ((s_keycount >= 2) && (ch != KEY_RET))
        {
            /* max input buffer limit */
            tty_putc(BELL);
        }
        else if (ch == KEY_RET)
        {
            if (!s_keycount)
            {
                show_menu();
                break;
            }
            /* process enter key state */
            if ((s_menuitem = search_menu_item(get_menu(), s_keybuf)) != NULL)
            {
                MENUITEM mitem;
                memcpy(&mitem, s_menuitem, sizeof(MENUITEM));

                /* If execute only, then go execute */
                if (mitem.menutype == MI_EXEC)
                {
                    if (mitem.exec)
                    {
                        if (!mitem.exec(s_menuitem))
                            tty_putc(BELL);
                        /* redraw the menu */
                        if (mitem.param2.U)
                            show_menu();
                    }
                }
                /* If new menu item type, then change menu */
                else if (mitem.menutype == MI_NMENU)
                {
                    change_menu(mitem.param1.U);
                    /* redraw the menu */
                    show_menu();
                }
                else
                {
                    /* If new menu item prompt, then show prompt
                     * and advance state for another enter keystroke.
                     */
                    prompt_menu_item(s_menuitem, 0);
                }
                s_keycount = 0;
            }
            else
            {
                /* bad menu input selection */
                tty_putc(BELL);
                /* redraw the menu */
                show_menu();
            }
            s_keycount = 0;
        }
        else if (isdigit(ch) || isalpha(ch))
        {
            do_bufkey(ch);
        }
        break;

    case ES_DATA_INPUT:
        if (ch == BKSPC)
        {
            do_backspace();
        }
        else if (is_escape(ch))
        {
            s_keycount = 0;
            s_edit_state = ES_MENU_SELECT;
            show_menu();
        }
        else if (ch == KEY_RET)
        {
            MENUITEM mitem;
            memcpy(&mitem, s_menuitem, sizeof(MENUITEM));

            /* check for empty key buffer */
            if (!s_keycount)
            {
                s_keycount = 0;
                s_edit_state = ES_MENU_SELECT;
                show_menu();
                break;
            }
            /* execute the menu handler */
            if (mitem.exec)
            {
                if (!mitem.exec(s_menuitem))
                    tty_putc(BELL);
            }

            /* redraw the menu */
            s_keycount = 0;
            s_edit_state = ES_MENU_SELECT;

            if (g_sys.debug)
                show_monitor_screen();
            else
                show_menu();
        }
        else if (ch == '.')
        {
            if (s_menuitem->datatype == DT_FLOAT)
            {
                /* only one decimal point allowed */
                if (strchr(s_keybuf, '.') == NULL)
                    do_bufkey(ch);
            }
        }
        else if (isdigit(ch) || isalpha(ch))
        {
            do_bufkey(ch);
        }
        else
        {
            tty_putc(BELL);
        }
        break;

    case ES_BITBOOL_SELECT:
        if (ch == ' ')
        {
            /* toggle and prompt next menu bitflag option */
            prompt_menu_item(s_menuitem, NEXT);
        }
        else if (is_escape(ch))
        {
            /* exit bitflag toggle mode */
            s_edit_state = ES_MENU_SELECT;
            s_keycount = 0;
            show_menu();
        }
        else if (ch == KEY_RET)
        {
            /* store the bitflag data */
            long data;
            /* read current 8 or 16 bit data */
            data = get_item_data(s_menuitem);
            /* mask out only affected flag bits */
            data &= ~(s_menuitem->param1.U);
            /* or in the new flag bit settings */
            data |= s_bitbool;
            /* store the final 8 or 16 bit result */
            set_item_data(s_menuitem, data);
            /* return to menu option select state */
            s_edit_state = ES_MENU_SELECT;
            s_keycount = 0;
            show_menu();
        }
        break;

    case ES_BITLIST_SELECT:
        if ((ch == 'N') || (ch == ' '))
        {
            /* toggle and prompt next menu bitflag option */
            prompt_menu_item(s_menuitem, NEXT);
        }
        else if (ch == 'P')
        {
            /* toggle and prompt prev menu bitflag option */
            prompt_menu_item(s_menuitem, PREV);
        }
        else if (is_escape(ch))
        {
            /* exit bitflag toggle mode */
            s_edit_state = ES_MENU_SELECT;
            s_keycount = 0;
            show_menu();
        }
        else if (ch == KEY_RET)
        {
            /* store the bitflag data */
            long data;
            /* read current 8 or 16 bit data */
            data = get_item_data(s_menuitem);
            /* mask out only affected flag bits */
            data &= ~(s_menuitem->param1.U);
            /* or in the new flag bit settings */
            data |= s_bitlist->value;
            /* store the final 8 or 16 bit result */
            set_item_data(s_menuitem, data);
            /* return to menu option select state */
            s_edit_state = ES_MENU_SELECT;
            s_keycount = 0;
            show_menu();
        }
        break;

    case ES_VALLIST_SELECT:
        if ((ch == 'N') || (ch == ' '))
        {
            /* toggle and prompt next menu bitflag option */
            prompt_menu_item(s_menuitem, NEXT);
        }
        else if (ch == 'P')
        {
            /* toggle and prompt prev menu vallist option */
            prompt_menu_item(s_menuitem, PREV);
        }
        else if (is_escape(ch))
        {
            /* exit bitflag toggle mode */
            s_edit_state = ES_MENU_SELECT;
            s_keycount = 0;
            show_menu();
        }
        else if (ch == KEY_RET)
        {
            /* store the bitflag data */
            long data;
            /* or in the new flag bit settings */
            data = s_vallist->value;
            /* store the final 8 or 16 bit result */
            set_item_data(s_menuitem, data);
            /* return to menu option select state */
            s_edit_state = ES_MENU_SELECT;
            s_keycount = 0;
            show_menu();
        }
        break;

    case ES_STRING_INPUT:
        if (ch == BKSPC)
        {
            do_backspace();
        }
        else if (is_escape(ch))
        {
            s_keycount = 0;
            s_edit_state = ES_MENU_SELECT;
            show_menu();
        }
        else if (ch == KEY_RET)
        {
            /* check for empty key buffer */
            if (!s_keycount)
            {
                s_keycount = 0;
                s_edit_state = ES_MENU_SELECT;
                show_menu();
                break;
            }
            /* store the text entered */
            set_item_text(s_menuitem, s_keybuf);

            /* apply any config changes to hardware */
            //apply_config();
            /* redraw the menu */
            s_keycount = 0;
            s_edit_state = ES_MENU_SELECT;
            show_menu();
        }
        else if (s_keycount <= s_max_chars)
        {
            /* Get the input format type */
            int fmt = s_menuitem->param2.U;

            /* Validate the key code */
            if (is_valid_key(ch, fmt))
                do_bufkey(ch);
            else
                tty_putc(BELL);
        }
        else
        {
            tty_putc(BELL);
        }
        break;

    default:
        break;
    }

    return res;
}

/*****************************************************************************
 * INTERNAL MENU HELPER FUNCTIONS
 *****************************************************************************/
/*
 * Handle enter key pressed from menu state.
 */

int do_hot_key(MENU *menu, int ch)
{
    if (isalpha(ch))
    {
        long i;
        MENUITEM* item = menu->items;

        for (i = 0; i < menu->count; i++)
        {
            if (item->optstr)
            {
                if (isalpha(*(item->optstr)))
                {
                    if (tolower(ch) == tolower(*(item->optstr)))
                    {
                        if ((item->menutype == MI_HOTKEY) && item->exec)
                        {
                            if (!item->exec(item))
                                tty_putc(BELL);

                            return 1;
                        }
                    }
                }
            }
            ++item;
        }
    }

    return 0;
}

void do_backspace(void)
{
    /* handle backspace key processing */
    if (s_keycount)
    {
        tty_putc('\b');
        tty_putc(' ');
        tty_putc('\b');

        s_keybuf[--s_keycount] = 0;
    }
    else
    {
        tty_putc(BELL);
    }
}

void do_bufkey(int ch)
{
    /* make sure we have room in the buffer */
    if (s_keycount < KEYBUF_SIZE - 1)
    {
        /* echo the character */
        tty_putc(ch);

        /* Store character and a null */
        s_keybuf[s_keycount] = (char) ch;
        s_keycount += 1;
        s_keybuf[s_keycount] = (char) 0;
    }
    else
    {
        /* beep if buffer is full */
        tty_putc(BELL);
    }
}

/* Check key codes against input format and return
 * the status indicating if the keycode is valid or not.
 */

int is_valid_key(int ch, int fmt)
{
    int rc = 1;
    return rc;
}

/* Test key code for valid escape keys. Currently we
 * accept <ESC> or 'X' to exit a menu or abort an operation.
 */
int is_escape(int ch)
{
	if ((ch == KEY_ESC) || (ch == toupper('x')))
		return 1;

	return 0;
}

/*
 * Return a pointer to current active menu data structure
 */

MENU* get_menu(void)
{
    return menu_tab[s_menu_num];
}

/*
 * Set the current active menu display state.
 */

void change_menu(long n)
{
    s_edit_state = ES_MENU_SELECT;

    s_keycount = 0;

    if (n >= 0 && n < NUM_MENUS)
        s_menu_num = n;
    else
        s_menu_num = MENU_MAIN;
}

/*
 * Scan menu table for option selection
 */

MENUITEM* search_menu_item(MENU* menu, char *optstr)
{
    int i;

    MENUITEM* item = (MENUITEM*) menu->items;

    int count = menu->count;

    for (i = 0; i < count; i++, item++)
    {
        if (strlen(item->optstr))
        {
            if (strcmp(optstr, item->optstr) == 0)
                return item;
        }
    }

    return NULL;
}


int get_hex_str(char* ptext, uint8_t* pdata, int len)
{
    char fmt[8];
    uint32_t i;
    int32_t l;

    *ptext = 0;
    strcpy(fmt, "%02X");

    for (i=0; i < len; i++)
    {
        l = sprintf(ptext, fmt, *pdata++);
        ptext += l;

        if (((i % 2) == 1) && (i != (len-1)))
        {
            l = sprintf(ptext, "-");
            ptext += l;
        }
    }

    return strlen(ptext);
}

/* Read data from the menu data item being edited.
 */

long get_item_data(MENUITEM* item)
{
    long data;

    char* pdata = (char*) item->data;

    if (item->datatype == DT_BYTE)
        data = (long) (*((char*) pdata));
    else if (item->datatype == DT_INT)
        data = (long) (*(int*) pdata);
    else if (item->datatype == DT_LONG)
        data = (long) (*((long*) pdata));
    else
        data = 0;

    return data;
}

/* Write data to the CONFIG memory structure for the size
 * and offset specified in the menu item structure.
 */

void set_item_data(MENUITEM* item, long data)
{
    if (item->datatype == DT_BYTE)
        *((char*)(item->data)) = data;
    else if (item->datatype == DT_INT)
        *((int*)(item->data)) = data;
    else if (item->datatype == DT_LONG)
        *((long*)(item->data)) = data;
}

void set_item_text(MENUITEM* item, char *text)
{
    strcpy((char*)(item->data), text);
}

/* This function scans an array of menu bitflags for a match against 'value'
 * and returns a pointer to the menu bitflag entry if found. The bitmask
 * member is used to specify which bits are compared.
 */

MENU_ARGLIST* find_bitlist_item(MENUITEM* item, long value)
{
    int i;
    long bitflag;

    MENU_ARGLIST* pal = (MENU_ARGLIST*) item->arglist;

    long mask  = item->param1.U;
    long items = item->param2.U;

    for (i = 0; i < items; i++)
    {
        bitflag = pal->value;

        if (bitflag == (mask & value))
            return pal;

        ++pal;
    }

    return NULL;
}

/* This function scans an array of discrete menu arg list entries for a match
 * against 'value' and returns a pointer to the menu arglist entry if found.
 */

MENU_ARGLIST* find_vallist_item(MENUITEM* item, long value)
{
    int i;
    long val;

    MENU_ARGLIST *pal = (MENU_ARGLIST*) item->arglist;

    long items = item->param2.U;

    for (i = 0; i < items; i++)
    {
        val = pal->value;

        if (value == val)
            return pal;

        ++pal;
    }

    return NULL;
}

/*
 * Show the input prompt for a menu item
 */

int prompt_menu_item(MENUITEM* item, int nextprev)
{
    int n;
    static char text[40];

    /* clear the prompt help line */
    if (!nextprev)
    {
        tty_pos(PROMPT_ROW - 2, PROMPT_COL);
        tty_erase_line();
    }

    /* clear the input line */
    tty_pos(PROMPT_ROW, PROMPT_COL);
    tty_erase_line();

    if (!strlen(item->optstr))
        return 0;

    /* remove any trailing spaces before prompting */

    memset(text, 0, sizeof(text));

    strncpy(text, item->text, sizeof(text)-2);

    if ((n = strlen(text)) > 0)
    {
        while (--n > 0)
        {
            if (text[n] == ' ')
                text[n] = '\0';
            else
                break;
        }
    }

    int type   = item->menutype;
    int param1 = item->param1.U;
    int param2 = item->param2.U;

    if ((type == MI_BITFLAG) || (type == MI_BITLIST) || (type == MI_VALLIST))
    {
        if (!nextprev)
        {
            tty_pos(PROMPT_ROW - 2, PROMPT_COL);
            tty_puts("<ENTER> Accept, ");

            if (type == MI_BITFLAG)
            	tty_puts("<SPACE> Toggle");
            else
            	tty_puts("<N>ext, <P>rev");

            tty_puts(" or <ESC> to cancel");
        }

        tty_pos(PROMPT_ROW, PROMPT_COL);
        tty_printf("Select %s : ", text);
    }

    if (type == MI_BITLIST)
    {
        if (s_bitlist && (nextprev == NEXT))
        {
            /* move to next item */
            MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
            MENU_ARGLIST* tail = head + param2;
            ++s_bitlist;
            if (s_bitlist >= tail)
                s_bitlist = head; /* wrap to head */
        }
        else if (s_bitlist && (nextprev == PREV))
        {
            /* move to prev item */
            MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
            MENU_ARGLIST* tail = head + param2 - 1;
            --s_bitlist;
            if (s_bitlist <= head)
                s_bitlist = tail; /* wrap to tail */
        }
        else
        {
            /* find current bitflag option and display */
            s_bitlist = find_bitlist_item(item, get_item_data(item));
        }

        tty_printf(s_inv_on);

        if (s_bitlist)
            tty_printf(s_bitlist->text);

        tty_printf(s_inv_off);

        /* enter toggle bitflag options state */
        s_edit_state = ES_BITLIST_SELECT;
    }
    else if (type == MI_VALLIST)
    {
        if (s_vallist && (nextprev == NEXT))
        {
            /* move to next item */
            MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
            MENU_ARGLIST* tail = head + param2;
            ++s_vallist;
            if (s_vallist >= tail)
                s_vallist = head; /* wrap to head */
        }
        else if (s_vallist && (nextprev == PREV))
        {
            /* move to prev item */
            MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
            MENU_ARGLIST* tail = head + param2 - 1;
            --s_vallist;
            if (s_vallist <= head)
                s_vallist = tail; /* wrap to tail */
        }
        else
        {
            /* find current bitflag option and display */
            s_vallist = find_vallist_item(item, get_item_data(item));
        }

        tty_printf(s_inv_on);

        if (s_vallist)
            tty_printf(s_vallist->text);

        tty_printf(s_inv_off);

        /* enter toggle value list options state */
        s_edit_state = ES_VALLIST_SELECT;
    }
    else if (type == MI_BITFLAG)
    {
        /* initial state */
        if (!nextprev)
        {
            /* initial state */
            s_bitbool = get_item_data(item);
        }
        else
        {
            /* toggle the bool bit state */
            if (s_bitbool & param1)
                s_bitbool &= ~(param1);
            else
                s_bitbool |= param1;
        }

        tty_printf(s_inv_on);
        tty_printf((s_bitbool & param1) ? "ON" : "OFF");
        tty_printf(s_inv_off);

        /* enter toggle bitflag options state */
        s_edit_state = ES_BITBOOL_SELECT;
    }
    else if (type == MI_STRING)
    {
        tty_pos(PROMPT_ROW, PROMPT_COL);

        tty_printf("Enter %s : ", text);

        /* Set max num of chars allowed for this item */
        s_max_chars = (int) param1;

        /* enter data input mode */
        s_edit_state = ES_STRING_INPUT;
    }
    else
    {
        tty_printf("Enter %s", text);

        if (type == MI_NUMERIC)
        {
			if (item->datatype == DT_FLOAT)
			{
				float fparam1 = item->param1.F;
				float fparam2 = item->param2.F;
				/* prompt with range low-high values */
				tty_printf(" (%.2f - %.2f): ", fparam1, fparam2);
			}
			else
			{
				/* prompt with range low-high values */
				tty_printf(" (%u - %u): ", param1, param2);
			}
        }
        else if (type == MI_VALLIST)
        {
            /* prompt with list of discrete values*/
            int i;

            long *values = (long*)item->arglist;

            tty_printf(" (");

            for (i = 0; i < param1; i++)
                tty_printf("%u,", values[i]);

            if (param1 > 1)
                tty_putc('\b');

            tty_printf("): ");
        }
        else
        {
            tty_printf(": ");
        }

        /* enter data input mode */
        s_edit_state = ES_DATA_INPUT;
    }

    return 1;
}

/*****************************************************************************
 * GLOBAL MENU EXECUTE HANDLER FUNCTIONS
 *****************************************************************************/

/* Default handler that converts input buffer string into a int or float
 * value and stores the data at the data pointer set in the menu table.
 */

int set_idata(MENUITEM* item)
{
    int rc = 0;
        
    /* Check for any string data in the key buffer */
    if (!strlen(s_keybuf))
        return 0;

    if (item->menutype != MI_NUMERIC)
        return 0;

    if (item->datatype == DT_FLOAT)
    {
        /* Get the numeric value in input buffer */
        float n = (float)atof(s_keybuf);

        /* Validate the value entered against the min/max
         * range values and set if within range.
         */
        if ((n >= item->param1.F) && (n <= item->param2.F))
        {
            if (item->datatype == DT_FLOAT)
                *((float*)item->data) = n;
            rc = 1;
        }
    }
    else
    {
        /* Get the numeric value in input buffer */
        long n = atol(s_keybuf);

        /* Validate the value entered against the min/max
         * range values and set if within range.
         */
        if ((n >= item->param1.U) && (n <= item->param2.U))
        {
            set_item_data(item, n);
            rc = 1;
        }
    }

    return rc;
}

/*
 * DIRECT EXECUTE MENU HANDLERS
 */

int mc_monitor_mode(MENUITEM *item)
{
    g_sys.debug = 1;
    show_monitor_screen();
    return 1;
}

int mc_cmd_stop(MENUITEM *item)
{
    (void)item;
    unsigned char bits = S_STOP;
    Mailbox_post(g_mailboxCommander, &bits, 10);
    return 1;
}

int mc_cmd_play(MENUITEM *item)
{
    (void)item;
    unsigned char bits = S_PLAY;
    Mailbox_post(g_mailboxCommander, &bits, 10);
    return 1;
}

int mc_cmd_fwd(MENUITEM *item)
{
    (void)item;
    unsigned char bits = S_FWD;
    Mailbox_post(g_mailboxCommander, &bits, 10);
    return 1;
}

int mc_cmd_rew(MENUITEM *item)
{
    (void)item;
    unsigned char bits = S_REW;
    Mailbox_post(g_mailboxCommander, &bits, 10);
    return 1;
}

int mc_write_config(MENUITEM *item)
{
	int ch;
	int rc;
    (void) item;

    tty_aputs(PROMPT_ROW, PROMPT_COL, "\t\t\t");

    tty_pos(PROMPT_ROW, PROMPT_COL);
    tty_puts("Save Config? (Y/N)");

    /* Wait for a keystroke */
    while (tty_getc(&ch) < 1);

    if (toupper(ch) == 'Y')
    {
		tty_pos(PROMPT_ROW, PROMPT_COL);

		if ((rc = SysParamsWrite(&g_sys)) != 0)
			tty_printf("ERROR %d : Writing Config Parameters...", rc);
		else
			tty_puts("Config parameters saved...");

		Task_sleep(1000);
    }

    show_menu();

    return 1;
}

int mc_read_config(MENUITEM *item)
{
    int ch;
	int rc;
    (void) item;

    tty_aputs(PROMPT_ROW, PROMPT_COL, "\t\t\t");

    tty_pos(PROMPT_ROW, PROMPT_COL);
    tty_puts("Recall Config? (Y/N)");

    /* Wait for a keystroke */
    while (tty_getc(&ch) < 1);

    if (toupper(ch) == 'Y')
    {
		/* Read the system config parameters from storage */
		if ((rc = SysParamsRead(&g_sys)) != 0)
		{
			tty_pos(PROMPT_ROW, PROMPT_COL);
			tty_printf("ERROR %d : Reading Config Parameters...", rc);
		}
		else
		{
			tty_pos(PROMPT_ROW, PROMPT_COL);
			tty_puts("Config parameters loaded...");
		}

		Task_sleep(1000);
    }

    show_menu();

    return 1;
}

int mc_default_config(MENUITEM *item)
{
	int ch;

    tty_aputs(PROMPT_ROW, PROMPT_COL, "\t\t\t");

    tty_pos(PROMPT_ROW, PROMPT_COL);
    tty_puts("Reset to Defaults? (Y/N)");

    /* Wait for a keystroke */
    while (tty_getc(&ch) < 1);

    if (toupper(ch) == 'Y')
    {
		// Initialize the default servo and program data values
		memset(&g_sys, 0, sizeof(SYSPARMS));
		InitSysDefaults(&g_sys);

		tty_pos(PROMPT_ROW, PROMPT_COL);
		tty_puts("All parameters reset to defaults...");

		Task_sleep(1000);
    }

    show_menu();

    return 1;
}

/*****************************************************************************
 * The following functions are for debug monitor support.
 *****************************************************************************/

void show_monitor_screen()
{
    /* Clear the screen and draw title */
    tty_cls();

    /* Show the menu title */
    tty_pos(1, 2);
    tty_printf(s_title, FIRMWARE_VER, FIRMWARE_REV);

    if (g_sys.debug == 1)
    {
        tty_pos(2, 64);
        tty_printf("%sMONITOR%s", s_inv_on, s_inv_off);

        tty_pos(3, 2);
        tty_printf("%sSUPPLY%s", s_ul_on, s_ul_off);
        tty_pos(4, 2);
        tty_puts("DAC Level");
        tty_pos(5, 2);
        tty_puts("Velocity");
        tty_pos(6, 2);
        tty_puts("Errors");
        tty_pos(7, 2);
        tty_puts("Stop Torque");
        tty_pos(8, 2);
        tty_puts("Offset");
        tty_pos(9, 2);
        tty_puts("Radius");

        tty_pos(3, 35);
        tty_printf("%sTAKEUP%s", s_ul_on, s_ul_off);
        tty_pos(4, 35);
        tty_puts("DAC Level");
        tty_pos(5, 35);
        tty_puts("Velocity");
        tty_pos(6, 35);
        tty_puts("Errors");
        tty_pos(7, 35);
        tty_puts("Stop Torque");
        tty_pos(8, 35);
        tty_puts("Offset");
        tty_pos(9, 35);
        tty_puts("Radius");

        tty_pos(11, 2);
        tty_printf("%sPID SERVO%s", s_ul_on, s_ul_off);
        tty_pos(12, 2);
        tty_puts("PID CV");
        tty_pos(13, 2);
        tty_puts("PID Error");
        tty_pos(14, 2);
        tty_puts("PID Debug");
        tty_pos(15, 2);
        tty_puts("Velocity");

        tty_pos(17, 2);
        tty_printf("%sTAPE%s", s_ul_on, s_ul_off);
        tty_pos(18, 2);
        tty_printf("Tape Tach");
        tty_pos(19, 2);
        tty_printf("Offset Null");
        tty_pos(20, 2);
        tty_printf("Tension Arm");

        tty_pos(12, 35);
        tty_puts("CPU Temp F");

        tty_pos(23, 2);
        tty_puts(s_escstr);
    }
    else
    {
        tty_printf("\r\n\n%s\r\n", s_escstr);
    }
}

static char get_dir_char(void)
{
    char ch;

    if (g_servo.direction == TAPE_DIR_REW) /* rev */
        ch = '<';
    else if (g_servo.direction == TAPE_DIR_FWD) /* fwd */
        ch = '>';
    else
        ch = '*';

    return ch;
}

void show_monitor_data()
{
    if (g_sys.debug == 1)
    {
        tty_pos(4, 25);
        tty_putc((int)get_dir_char());

        /* SUPPLY */
        tty_pos(4, 15);
        tty_printf(": %-4d", g_servo.dac_supply);
        tty_pos(5, 15);
        tty_printf(": %-8d", g_servo.velocity_supply);
        tty_pos(6, 15);
        tty_printf(": %-8u", g_servo.qei_supply_error_cnt);
        tty_pos(7, 15);
        tty_printf(": %-8d", g_servo.stop_torque_supply);
        tty_pos(8, 15);
        tty_printf(": %-8d", g_servo.offset_supply);
        tty_pos(9, 15);
        tty_printf(": %-8.2f", g_servo.radius_supply);

        /* TAKEUP */
        tty_pos(4, 49);
        tty_printf(": %-4d", g_servo.dac_takeup);
        tty_pos(5, 49);
        tty_printf(": %-8d", g_servo.velocity_takeup);
        tty_pos(6, 49);
        tty_printf(": %-8u", g_servo.qei_takeup_error_cnt);
        tty_pos(7, 49);
        tty_printf(": %-8d", g_servo.stop_torque_takeup);
        tty_pos(8, 49);
        tty_printf(": %-8d", g_servo.offset_takeup);
        tty_pos(9, 49);
        tty_printf(": %-8.2f", g_servo.radius_takeup);

        /* PID SERVO */
        tty_pos(12, 15);
        tty_printf(": %-12d", g_servo.db_cv);
        tty_pos(13, 15);
        tty_printf(": %-12d", g_servo.db_error);
        tty_pos(14, 15);
        tty_printf(": %-12d", g_servo.db_debug);
        tty_pos(15, 15);
        tty_printf(": %-12d", g_servo.velocity);

        /* TAPE */
        tty_pos(18, 15);
        tty_printf(": %-12d", g_servo.tape_tach);
        tty_pos(19, 15);
        tty_printf(": %-4d", g_servo.offset_null);
        tty_pos(20, 15);
        tty_printf(": %-8.1f", g_servo.tsense);

        tty_pos(12, 49);
        tty_printf(": %-8.1f", CELCIUS_TO_FAHRENHEIT(g_servo.cpu_temp));
    }
}

/* End-Of-File */
