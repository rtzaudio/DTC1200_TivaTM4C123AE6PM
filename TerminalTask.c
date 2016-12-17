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

/*-- STATIC FUNCTION PROTOTYPES --*/

static void show_home_menu(void);
static void show_menu(void);

static int do_menu_keystate(int key);
static int do_hot_key(MENU *menu, int ch);
static int is_valid_key(int ch, int fmt);
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

static char *get_item_text(MENUITEM* item);
static void set_item_text(MENUITEM* item, char *text);

static MENU_ARGLIST* find_bitlist_item(MENUITEM* item, long value);
static MENU_ARGLIST* find_vallist_item(MENUITEM* item, long value);

static int set_mdata(MENUITEM* item);

/* Menu Command Handlers */

int mc_cmd_stop(MENUITEM *item);
int mc_cmd_play(MENUITEM *item);
int mc_cmd_fwd(MENUITEM *item);
int mc_cmd_rew(MENUITEM *item);
int mc_default_config(MENUITEM *item);
int mc_read_config(MENUITEM *item);
int mc_write_config(MENUITEM *item);

/*-- MENU CONSTANTS --*/

/* Arg types for prompt_menu_item() */
#define NEXT                1
#define PREV                2

/* Edit mode states */
#define ES_MENU_SELECT      0       /* menu choice input selection state    */
#define ES_DATA_INPUT       1       /* numeric input state (range checked)  */
#define ES_STRING_INPUT     2       /* string input state (length checked)  */
#define ES_BITBOOL_SELECT   3       /* select on/off bitflag option state   */
#define ES_BITLIST_SELECT   4       /* select value from a list of bitflags */
#define ES_VALLIST_SELECT   5       /* select value from a discrete list    */

/*-- STATIC DATA ITEMS --*/

static char s_keybuf[KEYBUF_SIZE + 1]; /* keystroke input buffer */
static int s_keycount = 0; /* input key buffer count */
static int s_menu_num = 0; /* active menu number     */
static int s_edit_state = 0; /* current edit state     */
static int s_max_chars = 0;
static int s_end_debug = 0;

static MENUITEM* s_menuitem = NULL; /* current menu item ptr  */
static long s_bitbool = 0;

static MENU_ARGLIST* s_bitlist = NULL;
static MENU_ARGLIST* s_vallist = NULL;

/* VT100 Terminal Attributes */

static const char s_ul_on[] = VT100_UL_ON;
static const char s_ul_off[] = VT100_UL_OFF;
static const char s_inv_on[] = VT100_INV_ON;
static const char s_inv_off[] = VT100_INV_OFF;

static const char* s_escstr = "<ESC> Key Exits...";

static const char* s_title = "MM-1200 Transport/Servo Controller - v%u.%-2.2u";

static MENU_ARGLIST s_onoff[] = {
	{ "On", 	1,	},
	{ "Off",	0	}
};

/*
 *  Menu ID defines
 */

#define MENU_MAIN           0
#define MENU_DIAG           1
#define MENU_GENERAL        2
#define MENU_TENSION        3
#define MENU_STOP           4
#define MENU_SHUTTLE        5
#define MENU_PLAY           6

/*****************************************************************************
 * MAIN MENU ITEMS
 *****************************************************************************/

static MENUITEM main_items[] = {

{ 3, 5, "", "SETTINGS", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 1, "1", "General", MI_NMENU, MENU_GENERAL, 0, NULL, NULL, 0, 0 },

{ 6, 1, "2", "Tensions", MI_NMENU, MENU_TENSION, 0, NULL, NULL, 0, 0 },

{ 7, 1, "3", "Stop Servo", MI_NMENU, MENU_STOP, 0, NULL, NULL, 0, 0 },

{ 8, 1, "4", "Play Servo", MI_NMENU, MENU_PLAY, 0, NULL, NULL, 0, 0 },

{ 9, 1, "5", "Shuttle Servo", MI_NMENU, MENU_SHUTTLE, 0, NULL, NULL, 0, 0 },

{ 11, 5, "", "CONFIG", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 13, 1, "6", "Load Config", MI_EXEC, 0, 0, NULL, mc_read_config, 0, 0 },

{ 14, 1, "7", "Save Config", MI_EXEC, 0, 0, NULL, mc_write_config, 0, 0 },

{ 15, 1, "8", "Default Config", MI_EXEC, 0, 0, NULL, mc_default_config, 0, 0 },

{ 3, 35, "", "SYSTEM", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 30, "10", "Diagnostics", MI_NMENU, MENU_DIAG, 0, NULL, NULL, 0, 0 },

{ 7, 30, "11", "Monitor", MI_NRANGE, 0, 3, NULL, set_mdata, DT_LONG, &g_sys.debug },

{ 20, 1, "", "TRANSPORT:", MI_TEXT, 0, 1, NULL, NULL, 0, 0 },

{ 20, 12, "S", "<S>top", MI_HOTKEY, 1, 0, NULL, mc_cmd_stop, 0, 0 },

{ 20, 19, "P", "<P>lay", MI_HOTKEY, 1, 0, NULL, mc_cmd_play, 0, 0 },

{ 20, 26, "R", "<R>ewind", MI_HOTKEY, 1, 0, NULL, mc_cmd_rew, 0, 0 },

{ 20, 35, "F", "<F>orward", MI_HOTKEY, 1, 0, NULL, mc_cmd_fwd, 0, 0 },

{ PROMPT_ROW, PROMPT_COL, "", "Option: ", MI_PROMPT, 0, 0, NULL, NULL, 0, 0 } };

/* MAIN MENU */

static MENU menu_main = { MENU_MAIN, main_items, sizeof(main_items)
		/ sizeof(MENUITEM), "MAIN MENU" };

/*****************************************************************************
 * GENERAL MENU ITEMS
 *****************************************************************************/

static MENUITEM general_items[] = {

{ 3, 5, "", "GENERAL SETTINGS", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 1, "1", "Velocity Detect Threshold ", MI_NRANGE, 1, 50, NULL, set_mdata,
		DT_LONG, &g_sys.velocity_detect },

{ 6, 1, "2", "Torque Null Offset Gain   ", MI_NRANGE, 0, 5, NULL, set_mdata,
		DT_LONG, &g_sys.null_offset_gain },

{ 7, 1, "3", "Record Pulse Strobe Time  ", MI_NRANGE, 10, 100, NULL, set_mdata,
				DT_LONG, &g_sys.record_pulse_length },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT, 0, 0, NULL, NULL, 0, 0 } };

/* GENERAL MENU */

static MENU menu_general = { MENU_GENERAL, general_items, sizeof(general_items)
		/ sizeof(MENUITEM), "GENERAL MENU" };

/*****************************************************************************
 * TENSION MENU ITEMS
 *****************************************************************************/

#define MAX_TENSION		(DAC_MAX)

static MENUITEM tension_items[] = {

{ 3, 5, "", "SUPPLY TENSION", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 1, "1", "Stop    ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.stop_supply_tension },

{ 6, 1, "2", "Shuttle ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.shuttle_supply_tension },

{ 7, 1, "3", "Play LO ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.play_lo_supply_tension },

{ 8, 1, "4", "Play HI ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.play_hi_supply_tension },

{ 3, 30, "", "TAKEUP TENSION", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 26, "5", "Stop    ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.stop_takeup_tension },

{ 6, 26, "6", "Shuttle ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.shuttle_takeup_tension },

{ 7, 26, "7", "Play LO ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.play_lo_takeup_tension },

{ 8, 26, "8", "Play HI ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.play_hi_takeup_tension },

{ 10, 5, "", "MIN TORQUE", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 12, 1, "9", "Stop    ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.stop_min_torque },

{ 13, 1, "10", "Shuttle ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.shuttle_min_torque },

{ 14, 1, "11", "Play    ", MI_NRANGE, 1, MAX_TENSION, NULL, set_mdata, DT_LONG,
		&g_sys.play_min_torque },

{ 10, 30, "", "MAX TORQUE", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 12, 26, "12", "Stop    ", MI_NRANGE, 1, DAC_MAX, NULL, set_mdata, DT_LONG,
		&g_sys.stop_max_torque },

{ 13, 26, "13", "Shuttle ", MI_NRANGE, 1, DAC_MAX, NULL, set_mdata, DT_LONG,
		&g_sys.shuttle_max_torque },

{ 14, 26, "14", "Play    ", MI_NRANGE, 1, DAC_MAX, NULL, set_mdata, DT_LONG,
		&g_sys.play_max_torque },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT, 0, 0, NULL, NULL, 0, 0 } };

/* GENERAL MENU */

static MENU menu_tension = { MENU_TENSION, tension_items, sizeof(tension_items)
		/ sizeof(MENUITEM), "TENSION MENU" };

/*****************************************************************************
 * STOP MENU ITEMS
 *****************************************************************************/

static MENUITEM stop_items[] = {

{ 3, 5, "", "STOP SERVO", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 1, "1", "Stop Mode Brake Torque ", MI_NRANGE, 10, 512, NULL, set_mdata, DT_LONG,
		&g_sys.stop_brake_torque },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT, 0, 0, NULL, NULL, 0, 0 } };

static MENU menu_stop = { MENU_STOP, stop_items, sizeof(stop_items)
		/ sizeof(MENUITEM), "STOP MENU" };

/*****************************************************************************
 * SHUTTLE MENU ITEMS
 *****************************************************************************/

static MENUITEM shuttle_items[] = {

{ 3, 5, "", "SHUTTLE SERVO PID", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 1, "1", "P-Gain ", MI_NRANGE, 0, 500, NULL, set_mdata, DT_LONG,
		&g_sys.shuttle_servo_pgain },

{ 6, 1, "2", "I-Gain ", MI_NRANGE, 0, 500, NULL, set_mdata, DT_LONG,
		&g_sys.shuttle_servo_igain },

{ 7, 1, "3", "D-Gain ", MI_NRANGE, 0, 500, NULL, set_mdata, DT_LONG,
		&g_sys.shuttle_servo_dgain },

{ 9, 5, NULL, "SHUTTLE SETTINGS", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 11, 1, "7", "Shuttle Mode Velocity     ", MI_NRANGE, 50, 400, NULL, set_mdata,
		DT_LONG, &g_sys.shuttle_velocity },

{ 12, 1, "8", "Auto Decelerate Velocity  ", MI_NRANGE, 0, 200, NULL, set_mdata,
		DT_LONG, &g_sys.shuttle_slow_velocity },

{ 13, 1, "9", "Auto Decelerate at offset ", MI_NRANGE, 50, 100, NULL, set_mdata,
		DT_LONG, &g_sys.shuttle_slow_offset },

{ 14, 1, "10","Tape Lifter Settling Time ", MI_NRANGE, 0,2000, NULL, set_mdata,
		DT_LONG, &g_sys.lifter_settle_time },


{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT, 0, 0, NULL, NULL, 0, 0 } };

static MENU menu_shuttle = { MENU_SHUTTLE, shuttle_items, sizeof(shuttle_items)
		/ sizeof(MENUITEM), "SHUTTLE MENU" };

/*****************************************************************************
 * PLAY MENU ITEMS
 *****************************************************************************/

static MENUITEM play_items[] = {

{ 3, 5, NULL, "PLAY BOOST LO", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 1,  "1",  "Boost Time ", MI_NRANGE, 0, 2048, NULL, set_mdata, DT_LONG,
		&g_sys.play_lo_boost_time },

{ 6, 1,  "2",  "Boost Step ", MI_NRANGE, 0, 5, NULL, set_mdata, DT_LONG,
		&g_sys.play_lo_boost_step },

{ 7, 1,  "3",  "Boost Start", MI_NRANGE, 0, DAC_MAX, NULL, set_mdata, DT_LONG,
			&g_sys.play_lo_boost_start },

{ 8, 1,  "4",  "Boost End  ", MI_NRANGE, 0, DAC_MAX, NULL, set_mdata, DT_LONG,
			&g_sys.play_lo_boost_end },

{ 3, 38, NULL, "PLAY BOOST HI", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 34, "5",  "Boost Time ", MI_NRANGE, 0, 10000, NULL, set_mdata, DT_LONG,
		&g_sys.play_hi_boost_time },

{ 6, 34, "6",  "Boost Step ", MI_NRANGE, 1, 100, NULL, set_mdata, DT_LONG,
		&g_sys.play_hi_boost_step },

{ 7, 34, "7",  "Boost Start", MI_NRANGE, 0, DAC_MAX, NULL, set_mdata, DT_LONG,
		&g_sys.play_hi_boost_start },

{ 8, 34, "8",  "Boost End  ", MI_NRANGE, 0, DAC_MAX, NULL, set_mdata, DT_LONG,
		&g_sys.play_hi_boost_end },

{ 12, 5, NULL, "PLAY SETTINGS", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 14, 1, "10", "Play tension velocity gain   ", MI_NRANGE, 1, 24, NULL, set_mdata, DT_LONG,
		&g_sys.play_tension_gain },

{ 15, 1, "11", "Pinch Roller Settling Time   ", MI_NRANGE, 0, 1000, NULL, set_mdata, DT_LONG,
		&g_sys.pinch_settle_time },

{ 16, 1, "12", "Use Brakes to Stop Play Mode ", MI_VALLIST, 0, 2, s_onoff, NULL, DT_LONG,
		&g_sys.brakes_stop_play },

{ 17, 1, "13", "Engage Pinch Roller at Play  ", MI_VALLIST, 0, 2, s_onoff, NULL, DT_LONG,
		&g_sys.engage_pinch_roller },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT, 0, 0, NULL, NULL, 0, 0 } };

static MENU menu_play = { MENU_PLAY, play_items, sizeof(play_items)
		/ sizeof(MENUITEM), "PLAY MENU" };

/*****************************************************************************
 * DIAG MENU ITEMS
 *****************************************************************************/

static MENUITEM diag_items[] = {

{ 4, 5, NULL, "DIAGNOSTICS", MI_TEXT, 1, 0, NULL, NULL, 0, 0 },

{ 5, 1, "1", "Lamp Test", MI_EXEC, 0, 1, NULL, diag_lamp, 0, 0 },

{ 6, 1, "2", "Transport Test", MI_EXEC, 0, 1, NULL, diag_transport, 0, 0 },

{ 7, 1, "3", "MDA DAC Ramp Test", MI_EXEC, 0, 1, NULL, diag_dacramp, 0, 0 },

{ 8, 1, "4", "MDA DAC Zero Trim", MI_EXEC, 0, 1, NULL, diag_dacadjust, 0, 0 },
#if (CAPDATA_SIZE > 0)
{ 9, 1, "5", "Dump Capture Data", MI_EXEC, 0, 1, NULL, diag_dump_capture, 0, 0 },
#endif
{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT, 0, 1, NULL, NULL, 0, 0 } };

static MENU menu_diag = { MENU_DIAG, diag_items, sizeof(diag_items)
		/ sizeof(MENUITEM), "DIAGNOSTIC MENU" };

/*****************************************************************************
 * ARRAY OF ALL KNOWN MENUS
 *****************************************************************************/

/* Must be in order of MENU_XX ID defines! */

static MENU* menu_tab[] = { &menu_main, &menu_diag, &menu_general,
		&menu_tension, &menu_stop, &menu_shuttle, &menu_play, };

#define NUM_MENUS   (sizeof(menu_tab) / sizeof(MENU*))

/*****************************************************************************
 * TTY TERMINAL TASK
 *
 * This task drives the VT100 serial terminal port.
 *****************************************************************************/

#define RETKEY	(LF)

void Terminal_initialize(void)
{
	UART_Params uartParams;

	/* Open the UART port for the TTY console */

	UART_Params_init(&uartParams);

	uartParams.readMode       = UART_MODE_BLOCKING;
	uartParams.writeMode      = UART_MODE_BLOCKING;
	uartParams.readTimeout    = 1000;					// 1 second read timeout
	uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
	uartParams.readCallback   = NULL;
	uartParams.writeCallback  = NULL;
	uartParams.readReturnMode = UART_RETURN_NEWLINE;
	uartParams.writeDataMode  = UART_DATA_BINARY;
	uartParams.readDataMode   = UART_DATA_TEXT;
	uartParams.readEcho       = UART_ECHO_OFF;
	uartParams.baudRate       = 19200;					// 57600, 38400, 19200;
	uartParams.dataLength	  = UART_LEN_8;
	uartParams.parityType     = UART_PAR_NONE;
	uartParams.stopBits       = UART_STOP_ONE;

	g_handleUartTTY = UART_open(Board_UART_TTY, &uartParams);

	if (g_handleUartTTY == NULL)
		System_abort("Error initializing TTY UART\n");
}

/*****************************************************************************
 * TTY TERMINAL TASK
 *
 * This task drives the VT100 serial terminal port.
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
		if (s_end_debug) {
			s_end_debug = 0;
			s_edit_state = s_keycount = 0;
			show_menu();
			continue;
		}

		if (g_sys.debug)
		{
			if (ch == ESC)
			{
				if (g_sys.debug == 1) {
					g_sys.debug = 0;
					s_edit_state = s_keycount = 0;
					s_end_debug = 0;
					show_menu();
				} else {
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

	//unsigned stack;
    //stack = uxTaskGetStackHighWaterMark(NULL);

	tty_printf("DTC-1200 Transport Controller - v%d.%02d",
			FIRMWARE_VER, FIRMWARE_REV);

	/* Show the tape speed if main menu */

	if (menu->id == MENU_MAIN) {
		int speed = g_high_speed_flag ? 30 : 15;
		tty_pos(2, 78 - 6);
		tty_printf("%d IPS", speed);
	}

	/* Add the menu heading to top of screen */

	tty_pos(1, 78 - strlen(menu->heading));

	tty_printf(VT100_INV_ON);
	tty_printf(menu->heading);
	tty_printf(VT100_INV_OFF);

	/* Add the menu items text */

	int count = menu->count;

	MENUITEM* item = menu->items;

	for (i = 0; i < count; i++, item++) {
		long data = 0;

		/* preload any data member if it exists */

		if (item->datatype)
			data = get_item_data(item);

		int row = item->row;
		int col = item->col;
		int type = item->type;
		int parm1 = item->parm1;

		tty_pos(row, col);

		switch (type) {
		case MI_TEXT:
			if (parm1)
				/* display with underline */
				tty_printf("%s%s%s", VT100_UL_ON, item->text, VT100_UL_OFF);
			else
				/* display with normal */
				tty_printf("%s", item->text);
			break;

		case MI_EXEC:
		case MI_HOTKEY:
			if (parm1)
				tty_printf("%s", item->text);
			else
				tty_printf("%-2s) %s", item->optstr, item->text);
			break;

		case MI_PROMPT:
			if (!strlen(item->text)) {
				tty_printf("Option");

				/* append "esc exits" if not home menu */
				if (menu->id)
					tty_printf(" (ESC Exits)");

				tty_printf(": ");
			} else
				tty_printf("%s", item->text);
			break;

		case MI_NMENU:
			if (strlen(item->optstr))
				tty_printf("%-2s) %s", item->optstr, item->text);
			else
				tty_printf("%s", item->text);
			break;

		case MI_BITBOOL:
			tty_printf("%-2s) %s : ", item->optstr, item->text);
			if (data & item->parm1)
				tty_printf("ON");
			else
				tty_printf("OFF");
			break;

		case MI_NRANGE:
			tty_printf("%-2s) %s : %u", item->optstr, item->text, data);
			break;

		case MI_BITLIST:
			tty_printf("%-2s) %s : ", item->optstr, item->text);
			if ((pal = find_bitlist_item(item, data)) != NULL)
				tty_printf(pal->text);
			else
				tty_printf("???");
			break;

		case MI_VALLIST:
			tty_printf("%-2s) %s : ", item->optstr, item->text);
			if ((pal = find_vallist_item(item, data)) != NULL)
				tty_printf(pal->text);
			else
				tty_printf("???");
			break;

		case MI_STRING:
			tty_printf("%-2s) %s : %s", item->optstr, item->text,
					get_item_text(item));
			break;
		}
	}

	//tty_rxflush();
}

/*
 * Process a keystroke from the tty terminal input in single state mode.
 * Once the enter key is pressed, we process the menu item accordingly.
 */

int do_menu_keystate(int key)
{
	int res = 0;

	int ch = toupper(key);

	switch (s_edit_state) {
	case ES_MENU_SELECT:

		if (do_hot_key(get_menu(), ch)) {
			/* reset menu state and key count */
			s_edit_state = ES_MENU_SELECT;
			s_keycount = 0;
			show_menu();
			break;
		}

		if (ch == BKSPC) {
			do_backspace();
		} else if ((ch == ESC) && (s_menu_num != MENU_MAIN)) {
			change_menu(MENU_MAIN);
			show_menu();
		} else if ((s_keycount >= 2) && (ch != RETKEY)) {
			/* max input buffer limit */
			tty_putc(BELL);
		} else if (ch == RETKEY) {
			if (!s_keycount) {
				show_menu();
				break;
			}
			/* process enter key state */
			if ((s_menuitem = search_menu_item(get_menu(), s_keybuf)) != NULL) {
				MENUITEM mitem;
				memcpy(&mitem, s_menuitem, sizeof(MENUITEM));

				/* If execute only, then go execute */
				if (mitem.type == MI_EXEC) {
					if (mitem.exec) {
						if (!mitem.exec(s_menuitem))
							tty_putc(BELL);
						/* redraw the menu */
						if (mitem.parm2)
							show_menu();
					}
				}
				/* If new menu item type, then change menu */
				else if (mitem.type == MI_NMENU) {
					change_menu(mitem.parm1);
					/* redraw the menu */
					show_menu();
				} else {
					/* If new menu item prompt, then show prompt
					 * and advance state for another enter keystroke.
					 */
					prompt_menu_item(s_menuitem, 0);
				}
				s_keycount = 0;
			} else {
				/* bad menu input selection */
				tty_putc(BELL);
				/* redraw the menu */
				show_menu();
			}
			s_keycount = 0;
		} else if (isdigit(ch) || isalpha(ch)) {
			do_bufkey(ch);
		}
		break;

	case ES_DATA_INPUT:
		if (ch == BKSPC) {
			do_backspace();
		} else if (ch == ESC) {
			s_keycount = 0;
			s_edit_state = ES_MENU_SELECT;
			show_menu();
		} else if (ch == RETKEY) {
			MENUITEM mitem;
			memcpy(&mitem, s_menuitem, sizeof(MENUITEM));

			/* check for empty key buffer */
			if (!s_keycount) {
				s_keycount = 0;
				s_edit_state = ES_MENU_SELECT;
				show_menu();
				break;
			}
			/* execute the menu handler */
			if (mitem.exec) {
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
		} else if (isdigit(ch) || isalpha(ch)) {
			do_bufkey(ch);
		} else {
			tty_putc(BELL);
		}
		break;

	case ES_BITBOOL_SELECT:
		if (ch == ' ') {
			/* toggle and prompt next menu bitflag option */
			prompt_menu_item(s_menuitem, NEXT);
		} else if (ch == ESC) {
			/* exit bitflag toggle mode */
			s_edit_state = ES_MENU_SELECT;
			s_keycount = 0;
			show_menu();
		} else if (ch == RETKEY) {
			/* store the bitflag data */
			long data;
			/* read current 8 or 16 bit data */
			data = get_item_data(s_menuitem);
			/* mask out only affected flag bits */
			data &= ~(s_menuitem->parm1);
			/* or in the new flag bit settings */
			data |= s_bitbool;
			/* store the final 8 or 16 bit result */
			set_item_data(s_menuitem, data);

			/* apply any config changes to hardware */
			//apply_config();
			/* return to menu option select state */
			s_edit_state = ES_MENU_SELECT;
			s_keycount = 0;
			show_menu();
		}
		break;

	case ES_BITLIST_SELECT:
		if ((ch == 'N') || (ch == ' ')) {
			/* toggle and prompt next menu bitflag option */
			prompt_menu_item(s_menuitem, NEXT);
		} else if (ch == 'P') {
			/* toggle and prompt prev menu bitflag option */
			prompt_menu_item(s_menuitem, PREV);
		} else if (ch == ESC) {
			/* exit bitflag toggle mode */
			s_edit_state = ES_MENU_SELECT;
			s_keycount = 0;
			show_menu();
		} else if (ch == RETKEY) {
			/* store the bitflag data */
			long data;
			/* read current 8 or 16 bit data */
			data = get_item_data(s_menuitem);
			/* mask out only affected flag bits */
			data &= ~(s_menuitem->parm1);
			/* or in the new flag bit settings */
			data |= s_bitlist->value;
			/* store the final 8 or 16 bit result */
			set_item_data(s_menuitem, data);

			/* apply any config changes to hardware */
			//apply_config();
			/* return to menu option select state */
			s_edit_state = ES_MENU_SELECT;
			s_keycount = 0;
			show_menu();
		}
		break;

	case ES_VALLIST_SELECT:
		if ((ch == 'N') || (ch == ' ')) {
			/* toggle and prompt next menu bitflag option */
			prompt_menu_item(s_menuitem, NEXT);
		} else if (ch == 'P') {
			/* toggle and prompt prev menu vallist option */
			prompt_menu_item(s_menuitem, PREV);
		} else if (ch == ESC) {
			/* exit bitflag toggle mode */
			s_edit_state = ES_MENU_SELECT;
			s_keycount = 0;
			show_menu();
		} else if (ch == RETKEY) {
			/* store the bitflag data */
			long data;
			/* or in the new flag bit settings */
			data = s_vallist->value;
			/* store the final 8 or 16 bit result */
			set_item_data(s_menuitem, data);

			/* apply any config changes to hardware */
			//apply_config();
			/* return to menu option select state */
			s_edit_state = ES_MENU_SELECT;
			s_keycount = 0;
			show_menu();
		}
		break;

	case ES_STRING_INPUT:
		if (ch == BKSPC) {
			do_backspace();
		} else if (ch == ESC) {
			s_keycount = 0;
			s_edit_state = ES_MENU_SELECT;
			show_menu();
		} else if (ch == RETKEY) {
			/* check for empty key buffer */
			if (!s_keycount) {
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
		} else if (s_keycount <= s_max_chars) {
			/* Get the input format type */
			int fmt = s_menuitem->parm2;

			/* Validate the key code */
			if (is_valid_key(ch, fmt))
				do_bufkey(ch);
			else
				tty_putc(BELL);
		} else {
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
	if (isalpha(ch)) {
		long i;
		MENUITEM* item = menu->items;

		for (i = 0; i < menu->count; i++) {
			if (item->optstr) {
				if (isalpha(*(item->optstr))) {
					if (tolower(ch) == tolower(*(item->optstr))) {
						if ((item->type == MI_HOTKEY) && item->exec) {
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
	if (s_keycount) {
		tty_putc('\b');
		tty_putc(' ');
		tty_putc('\b');

		s_keybuf[--s_keycount] = 0;
	} else {
		tty_putc(BELL);
	}
}

void do_bufkey(int ch)
{
	/* make sure we have room in the buffer */
	if (s_keycount < KEYBUF_SIZE - 1) {
		/* echo the character */
		tty_putc(ch);

		/* Store character and a null */
		s_keybuf[s_keycount] = (char) ch;
		s_keycount += 1;
		s_keybuf[s_keycount] = (char) 0;
	} else {
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

	for (i = 0; i < count; i++, item++) {
		if (strlen(item->optstr)) {
			if (strcmp(optstr, item->optstr) == 0)
				return item;
		}
	}

	return NULL;
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

char *get_item_text(MENUITEM* item)
{
	return (char*) (item->data);
}

/* Write data to the CONFIG memory structure for the size
 * and offset specified in the menu item structure.
 */

void set_item_data(MENUITEM* item, long data)
{
	char* pdata = (char*) item->data;

	if (item->datatype == DT_BYTE)
		*((char*) pdata) = data;
	else if (item->datatype == DT_INT)
		*((int*) pdata) = data;
	else if (item->datatype == DT_LONG)
		*((long*) pdata) = data;
}

void set_item_text(MENUITEM* item, char *text)
{
	strcpy((char*) (item->data), text);
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

	long mask = item->parm1;
	long items = item->parm2;

	for (i = 0; i < items; i++) {
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

	long items = item->parm2;

	for (i = 0; i < items; i++) {
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
	if (!nextprev) {
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

	if ((n = strlen(text)) > 0) {
		while (--n > 0) {
			if (text[n] == ' ')
				text[n] = '\0';
			else
				break;
		}
	}

	int type = item->type;
	int parm1 = item->parm1;
	int parm2 = item->parm2;

	if ((type == MI_BITBOOL) || (type == MI_BITLIST) || (type == MI_VALLIST)) {
		if (!nextprev) {
			tty_pos(PROMPT_ROW - 2, PROMPT_COL);
			tty_printf("<ENTER> to accept, <N>ext, <P>rev or <ESC> to cancel");
		}

		tty_pos(PROMPT_ROW, PROMPT_COL);
		tty_printf("Select %s : ", text);
	}

	if (type == MI_BITLIST) {
		if (s_bitlist && (nextprev == NEXT)) {
			/* move to next item */
			MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
			MENU_ARGLIST* tail = head + parm2;
			++s_bitlist;
			if (s_bitlist >= tail)
				s_bitlist = head; /* wrap to head */
		} else if (s_bitlist && (nextprev == PREV)) {
			/* move to prev item */
			MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
			MENU_ARGLIST* tail = head + parm2 - 1;
			--s_bitlist;
			if (s_bitlist <= head)
				s_bitlist = tail; /* wrap to tail */
		} else {
			/* find current bitflag option and display */
			s_bitlist = find_bitlist_item(item, get_item_data(item));
		}

		tty_printf(VT100_INV_ON);

		if (s_bitlist)
			tty_printf(s_bitlist->text);
		else
			tty_printf("???");

		tty_printf(VT100_INV_OFF);

		/* enter toggle bitflag options state */
		s_edit_state = ES_BITLIST_SELECT;
	} else if (type == MI_VALLIST) {
		if (s_vallist && (nextprev == NEXT)) {
			/* move to next item */
			MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
			MENU_ARGLIST* tail = head + parm2;
			++s_vallist;
			if (s_vallist >= tail)
				s_vallist = head; /* wrap to head */
		} else if (s_vallist && (nextprev == PREV)) {
			/* move to prev item */
			MENU_ARGLIST* head = (MENU_ARGLIST*) item->arglist;
			MENU_ARGLIST* tail = head + parm2 - 1;
			--s_vallist;
			if (s_vallist <= head)
				s_vallist = tail; /* wrap to tail */
		} else {
			/* find current bitflag option and display */
			s_vallist = find_vallist_item(item, get_item_data(item));
		}

		tty_printf(VT100_INV_ON);

		if (s_vallist)
			tty_printf(s_vallist->text);
		else
			tty_printf("???");

		tty_printf(VT100_INV_OFF);

		/* enter toggle value list options state */
		s_edit_state = ES_VALLIST_SELECT;
	} else if (type == MI_BITBOOL) {
		/* initial state */
		if (!nextprev) {
			/* initial state */
			s_bitbool = get_item_data(item);
		} else {
			/* toggle the bool bit state */
			if (s_bitbool & parm1)
				s_bitbool &= ~(parm1);
			else
				s_bitbool |= parm1;
		}

		tty_printf(VT100_INV_ON);

		if (s_bitbool & parm1)
			tty_printf("ON");
		else
			tty_printf("OFF");

		tty_printf(VT100_INV_OFF);

		/* enter toggle bitflag options state */
		s_edit_state = ES_BITBOOL_SELECT;
	} else if (type == MI_STRING) {
		tty_pos(PROMPT_ROW, PROMPT_COL);

		tty_printf("Enter %s : ", text);

		/* Set max num of chars allowed for this item */
		s_max_chars = (int) parm1;

		/* enter data input mode */
		s_edit_state = ES_STRING_INPUT;
	} else {
		tty_printf("Enter %s", text);

		if (type == MI_NRANGE) {
			/* prompt with range low-high values */
			tty_printf(" (%u-%u): ", parm1, parm2);
		} else if (type == MI_VALLIST) {
			/* prompt with list of discrete values*/
			int i;

			long *values = (long*) item->arglist;

			tty_printf(" (");

			for (i = 0; i < parm1; i++)
				tty_printf("%u,", values[i]);

			if (parm1 > 1)
				tty_putc('\b');

			tty_printf("): ");
		} else {
			tty_printf(": ");
		}

		/* enter data input mode */
		s_edit_state = ES_DATA_INPUT;
	}

	return 1;
}

/*****************************************************************************
 * MENU EXECUTE HANDLER FUNCTIONS
 *****************************************************************************/

/* Default handler that converts input buffer string into a int value
 * and sets the data item from the pointer in the menu table.
 */

int set_mdata(MENUITEM* item)
{
	int rc = 0;

	//if (!pgm_read_word(&item->datasize))
	//  return 0;

	int type = item->type;
	int parm1 = item->parm1;
	int parm2 = item->parm2;

	if (type == MI_NRANGE) {
		long n;

		/* Check for return numeric data item value */
		if (strlen(s_keybuf)) {
			/* Get the numeric value in input buffer */
			n = atol(s_keybuf);

			/* Validate the value entered against the min/max
			 * range values and set if within range.
			 */
			if ((n >= parm1) && (n <= parm2)) {
				set_item_data(item, n);
				rc = 1;
			}
		}
	}

	return rc;
}

/*
 * DIRECT EXECUTE MENU HANDLERS
 */

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
	int rc;
	(void) item;

	tty_aputs(PROMPT_ROW, PROMPT_COL, "\t\t\t");

	if ((rc = SysParamsWrite(&g_sys)) != 0) {
		tty_pos(PROMPT_ROW, PROMPT_COL);
		tty_printf("ERROR %d  Writing Config Parameters...", rc);
	} else {
		tty_pos(PROMPT_ROW, PROMPT_COL);
		tty_puts("Config Parameters Saved...");
	}

	Task_sleep(1000);
	show_menu();

	return 1;
}

int mc_read_config(MENUITEM *item)
{
	int rc;
	(void) item;

	tty_aputs(PROMPT_ROW, PROMPT_COL, "\t\t\t");

	/* Read the system config parameters from storage */
	if ((rc = SysParamsRead(&g_sys)) != 0) {
		tty_pos(PROMPT_ROW, PROMPT_COL);
		tty_printf("ERROR %d  Reading Config Parameters...", rc);
	} else {
		tty_pos(PROMPT_ROW, PROMPT_COL);
		tty_puts("Config Parameters Loaded...");
	}

	Task_sleep(1000);
	show_menu();

	return 1;
}

int mc_default_config(MENUITEM *item)
{
	tty_aputs(PROMPT_ROW, PROMPT_COL, "\t\t\t");

    // Initialize the default servo and program data values
    memset(&g_sys, 0, sizeof(SYSPARMS));
    InitSysDefaults(&g_sys);

    tty_pos(PROMPT_ROW, PROMPT_COL);
	tty_puts("Default config set...");
	Task_sleep(1000);

	show_menu();
	return 1;
}

/*
 * The following functions are for debug monitor support.
 */

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

void show_monitor_screen()
{
	/* Clear the screen and draw title */
	tty_cls();

	/* Show the menu title */
	tty_printf(s_title, FIRMWARE_VER, FIRMWARE_REV);

	if (g_sys.debug == 1) {
		tty_pos(1, 64);
		tty_printf("%sMONITOR%s %u", s_inv_on, s_inv_off, g_sys.debug);

		tty_pos(4, 1);
		tty_printf("%sSUPPLY%s", s_ul_on, s_ul_off);
		tty_pos(5, 1);
		tty_puts("DAC Level   :");
		tty_pos(6, 1);
		tty_puts("Velocity    :");
		tty_pos(7, 1);
		tty_puts("RPM         :");
		tty_pos(8, 1);
		tty_puts("Stop Null   :");
		tty_pos(9, 1);
		tty_puts("Offset      :");

		tty_pos(4, 35);
		tty_printf("%sTAKEUP%s", s_ul_on, s_ul_off);
		tty_pos(5, 35);
		tty_puts("DAC Level   :");
		tty_pos(6, 35);
		tty_puts("Velocity    :");
		tty_pos(7, 35);
		tty_puts("RPM         :");
		tty_pos(8, 35);
		tty_puts("Stop Null   :");
		tty_pos(9, 35);
		tty_puts("Offset      :");

		tty_pos(11, 1);
		tty_printf("%sPID SERVO%s", s_ul_on, s_ul_off);
		tty_pos(12, 1);
		tty_puts("PID CV      :");
		tty_pos(13, 1);
		tty_puts("PID Error   :");
		tty_pos(14, 1);
		tty_puts("PID Debug   :");
		tty_pos(15, 1);
		tty_puts("Velocity    :");

		tty_pos(17, 1);
		tty_printf("%sTAPE%s", s_ul_on, s_ul_off);
		tty_pos(18, 1);
		tty_printf("Tape Tach   :");
		tty_pos(19, 1);
		tty_printf("Offset Null :");
		tty_pos(20, 1);
		tty_printf("Tension Arm :");

		tty_pos(23, 1);
		tty_puts(s_escstr);
	} else {
		tty_printf("\r\n\n%s\r\n", s_escstr);
	}
}

void show_monitor_data()
{
	if (g_sys.debug == 1) {

		tty_pos(4, 25);
		tty_putc((int)get_dir_char());

		/* SUPPLY */
		tty_pos(5, 15);
		tty_printf("%-4d", g_servo.dac_supply);
		tty_pos(6, 15);
		tty_printf("%-8d", g_servo.velocity_supply);
		tty_pos(7, 15);
		tty_printf("%-8d", RPM(g_servo.velocity_supply));
		tty_pos(8, 15);
		tty_printf("%-8d", g_servo.stop_null_supply);
		tty_pos(9, 15);
		tty_printf("%-8d", g_servo.offset_supply);

		/* TAKEUP */
		tty_pos(5, 49);
		tty_printf("%-4d", g_servo.dac_takeup);
		tty_pos(6, 49);
		tty_printf("%-8d", g_servo.velocity_takeup);
		tty_pos(7, 49);
		tty_printf("%-8d", RPM(g_servo.velocity_takeup));
		tty_pos(8, 49);
		tty_printf("%-8d", g_servo.stop_null_takeup);
		tty_pos(9, 49);
		tty_printf("%-8d", g_servo.offset_takeup);

		/* PID SERVO */
		tty_pos(12, 15);
		tty_printf("%-12d", g_servo.db_cv);
		tty_pos(13, 15);
		tty_printf("%-12d", g_servo.db_error);
		tty_pos(14, 15);
		tty_printf("%-12d", g_servo.db_debug);
		tty_pos(15, 15);
		tty_printf("%-12d", g_servo.velocity);

		/* TAPE */
		tty_pos(18, 15);
		tty_printf("%-12d", g_servo.tape_tach);
		tty_pos(19, 15);
		tty_printf("%-4d", g_servo.offset_null);
		tty_pos(20, 15);
		tty_printf("%-4d", g_servo.tsense);

	} else if (g_sys.debug == 2) {
		tty_printf(
				"SVel=%-4.4d%c TVel=%-4.4d%c Vel=%-4.4d, Tach=%-4.4d, NullOff=%-4.4d, Debug=%d\r\n",
				g_servo.velocity_supply, get_dir_char(),
				g_servo.velocity_takeup, get_dir_char(),
				g_servo.velocity, g_servo.tape_tach, g_servo.offset_null,
				g_servo.db_debug);
	} else if (g_sys.debug == 3) {
		tty_printf(
				"SDAC=%-4.4d TDAC=%-4.4d CV=%4.4d Vel=%-4.4d Err=%ld\r\n",
				g_servo.dac_supply, g_servo.dac_takeup, g_servo.db_cv,
				g_servo.velocity, g_servo.db_error);

	}
}

/* End-Of-File */
