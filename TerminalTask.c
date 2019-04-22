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
#define ES_BITFLAG_SELECT   3       /* select on/off bitflag option state   */
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

const char g_ul_on[]   = VT100_UL_ON;
const char g_ul_off[]  = VT100_UL_OFF;
const char g_inv_on[]  = VT100_INV_ON;
const char g_inv_off[] = VT100_INV_OFF;
const char g_escstr[]  = "<ESC> or 'X' to exit...";
const char g_title[]   = "DTC-1200 Transport Controller v%u.%-2.2u.%-3.3u";

/*****************************************************************************
 * STATIC FUNCTION PROTOTYPES
 *****************************************************************************/

static int do_menu_keystate(int key);
static int do_hot_key(MENU *menu, int ch);
static int is_valid_key(int ch, int fmt);
static int is_escape(int ch);
static void change_menu(long n);
static void do_backspace(void);
static void do_bufkey(int ch);

static MENU* get_menu(void);
static MENUITEM* search_menu_item(MENU* menu, char *optstr);
static int prompt_menu_item(MENUITEM* item, int nextprev);
static void prompt_edit_prefix(char* prompt, bool suffix);
static long get_item_data(MENUITEM* item);
static void set_item_data(MENUITEM* item, long data);
static void set_item_text(MENUITEM* item, char *text);
static MENU_ARGLIST* find_bitlist_item(MENUITEM* item, long value);
static MENU_ARGLIST* find_vallist_item(MENUITEM* item, long value);
static int get_hex_str(char* textbuf, uint8_t* databuf, int len);

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
    int ch;

    /* Initialize the RS-232 port for VT100 terminal emulation */
    Terminal_initialize();

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
    uint32_t baud = (g_dip_switch & M_DIPSW1) ? 115200: 19200;

    /* Open the UART port for the TTY console */

    UART_Params_init(&uartParams);

    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = (baud > 19200) ? 250 : 500;     // 0.5 second read timeout
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
    tty_printf(g_title, FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);

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
        tty_pos(20, 2);
        tty_printf("Serial# %s", buf);
    }

    /* Add the menu heading to top of screen */

    tty_pos(2, 78 - strlen(menu->heading));

    tty_printf(g_inv_on);
    tty_printf(menu->heading);
    tty_printf(g_inv_off);

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

        int type   = item->menutype;
        int param1 = item->param1.U;

        /* Set the starting cursor position */
        tty_pos(item->row, item->col);

        switch (type)
        {
        case MI_TEXT:
            if (param1)
                /* display with underline */
                tty_printf("%s%s%s", g_ul_on, item->menutext, g_ul_off);
            else
                /* display with normal */
                tty_printf("%s", item->menutext);
            break;

        case MI_EXEC:
        case MI_HOTKEY:
            if (param1)
                tty_printf("%s", item->menutext);
            else
                tty_printf("%2s) %s", item->menuopt, item->menutext);
            break;

        case MI_PROMPT:
            if (!strlen(item->menutext))
            {
                tty_printf("Option");

                /* append "esc exits" if not home menu */
                if (menu->id)
                    tty_printf(" (ESC or 'X' to exit)");

                tty_printf(": ");
            }
            else
                tty_printf("%s", item->menutext);
            break;

        case MI_NMENU:
            if (strlen(item->menuopt))
                tty_printf("%2s) %s", item->menuopt, item->menutext);
            else
                tty_printf("%s", item->menutext);
            break;

        case MI_BITFLAG:
            tty_printf("%2s) %s : ", item->menuopt, item->menutext);
            if (data & item->param1.U)
                tty_printf("ON");
            else
                tty_printf("OFF");
            break;

        case MI_NUMERIC:
        	if (item->datatype == DT_FLOAT)
        	{
        		fval = (float)(*((float*) item->data));
        		tty_printf("%2s) %s : %.3f", item->menuopt, item->menutext, fval);
        	}
        	else
        	{
        		tty_printf("%2s) %s : %u", item->menuopt, item->menutext, data);
        	}
            break;

        case MI_BITLIST:
            tty_printf("%2s) %s : ", item->menuopt, item->menutext);
            if ((pal = find_bitlist_item(item, data)) != NULL)
                tty_printf(pal->text);
            break;

        case MI_VALLIST:
            tty_printf("%2s) %s : ", item->menuopt, item->menutext);
            if ((pal = find_vallist_item(item, data)) != NULL)
                tty_printf(pal->text);
            break;

        case MI_STRING:
            tty_printf("%2s) %s : %s", item->menuopt, item->menutext,
                    (char*)(item->data));
            break;
        }
    }
}

/*
 * Helper to display edit mode prefix and prompt string
 */

void prompt_edit_prefix(char* prompt, bool suffix)
{
    tty_pos(PROMPT_ROW, PROMPT_COL);

	tty_puts(g_inv_on);
	tty_puts("EDIT:");
	tty_puts(g_inv_off);

	tty_putc(' ');
	tty_puts(prompt);

	if (suffix)
		tty_puts(" : ");
}

/*
 * Show the input prompt for a menu item
 */

int prompt_menu_item(MENUITEM* item, int nextprev)
{
    int n;
    static char text[80];

    /* clear the prompt help line */
    if (!nextprev)
    {
        tty_pos(PROMPT_ROW - 2, PROMPT_COL);
        tty_erase_line();
    }

    /* clear the input line */
    tty_pos(PROMPT_ROW, PROMPT_COL);
    tty_erase_line();

    if (!strlen(item->menuopt))
        return 0;

    /* remove any trailing spaces before prompting */

    memset(text, 0, sizeof(text));

    strncpy(text, item->menutext, sizeof(text)-2);

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
            tty_puts("<ENTER>=Accept, ");
           	tty_puts((type == MI_BITFLAG) ? "<SPACE>=Toggle" : "<N>ext, <P>rev");
            tty_puts(" or <ESC> to cancel");
        }

        //tty_pos(PROMPT_ROW, PROMPT_COL);
        //tty_printf("[EDIT] %s : ", text);
        prompt_edit_prefix(text, 1);
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

        tty_printf(g_inv_on);

        if (s_bitlist)
            tty_printf(s_bitlist->text);

        tty_printf(g_inv_off);

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

        tty_printf(g_inv_on);

        if (s_vallist)
            tty_printf(s_vallist->text);

        tty_printf(g_inv_off);

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

        tty_printf(g_inv_on);
        tty_printf((s_bitbool & param1) ? "ON" : "OFF");
        tty_printf(g_inv_off);

        /* enter toggle bitflag options state */
        s_edit_state = ES_BITFLAG_SELECT;
    }
    else if (type == MI_STRING)
    {
        //tty_pos(PROMPT_ROW, PROMPT_COL);
        //tty_printf("[EDIT] %s : ", text);

        prompt_edit_prefix(text, 1);

        /* Set max num of chars allowed for this item */
        s_max_chars = (int) param1;

        /* enter data input mode */
        s_edit_state = ES_STRING_INPUT;
    }
    else
    {
        //tty_printf("[EDIT] %s", text);

    	prompt_edit_prefix(text, 0);

        if (type == MI_NUMERIC)
        {
			if (item->datatype == DT_FLOAT)
			{
				float fparam1 = item->param1.F;
				float fparam2 = item->param2.F;
				/* prompt with range low-high values */
				tty_printf(" (%.3f - %.3f): ", fparam1, fparam2);
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

    case ES_BITFLAG_SELECT:
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

        for (i=0; i < menu->count; i++)
        {
            if (item->menuopt)
            {
                if (isalpha(*(item->menuopt)))
                {
                    if (tolower(ch) == tolower(*(item->menuopt)))
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
        if (strlen(item->menuopt))
        {
            if (strcmp(optstr, item->menuopt) == 0)
                return item;
        }
    }

    return NULL;
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

/* Format hex string for GUID serial number display.
 */

int get_hex_str(char* textbuf, uint8_t* databuf, int len)
{
    char *p = textbuf;
    uint8_t *d;
    uint32_t i;
    int32_t l;

    /* Null output text buffer initially */
    *textbuf = 0;

    /* Make sure buffer length is not zero */
    if (!len)
        return 0;

    /* Read data bytes in reverse order so we print most significant byte first */
    d = databuf + (len-1);

    for (i=0; i < len; i++)
    {
        l = sprintf(p, "%02X", *d--);
        p += l;

        if (((i % 2) == 1) && (i != (len-1)))
        {
            l = sprintf(p, "-");
            p += l;
        }
    }

    return strlen(textbuf);
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

/* Default menu handler that converts input buffer string into a int or float
 * value and stores the data at the data pointer set in the menu table.
 */

int put_idata(MENUITEM* item)
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

/* End-Of-File */
