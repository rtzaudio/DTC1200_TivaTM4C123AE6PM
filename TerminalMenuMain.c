/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
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

/*****************************************************************************
 * MAIN MENU ITEMS
 *****************************************************************************/

static MENUITEM main_items[] = {

{ 3, 6, "", "SETTINGS", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 2, "1", "General", MI_NMENU,
		.param1.U = MENU_GENERAL,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 6, 2, "2", "Tensions", MI_NMENU,
		.param1.U = MENU_TENSION,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 7, 2, "3", "Stop Servo", MI_NMENU,
		.param1.U = MENU_STOP,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 8, 2, "4", "Play Servo", MI_NMENU,
		.param1.U = MENU_PLAY,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 9, 2, "5", "Shuttle Servo", MI_NMENU,
		.param1.U = MENU_SHUTTLE,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 11, 6, "", "CONFIG", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 13, 2, "6", "Save Current Config", MI_EXEC,
		.param1.U = 0,
		.param2.U = 0,
		NULL, mc_write_config, 0, 0 },

{ 14, 2, "7", "Recall Saved Config", MI_EXEC,
		.param1.U = 0,
		.param2.U = 0,
		NULL, mc_read_config, 0, 0 },

{ 15, 2, "8", "Reset to Defaults", MI_EXEC,
		.param1.U = 0,
		.param2.U = 0,
		NULL, mc_default_config, 0, 0 },

{ 3, 34, "", "SYSTEM", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 30, "10", "Diagnostics", MI_NMENU,
		.param1.U = MENU_DIAG,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 6, 30, "11", "Monitor Screen", MI_EXEC,
		.param1.U = 0,
		.param2.U = 0,
		NULL, mc_monitor_mode, 0, 0 },

{ 20, 2, "", "TRANSPORT:", MI_TEXT,
		.param1.U = 0,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 20, 13, "S", "<S>top", MI_HOTKEY,
		.param1.U = 1,
		.param2.U = 0,
		NULL, mc_cmd_stop, 0, 0 },

{ 20, 20, "P", "<P>lay", MI_HOTKEY,
		.param1.U = 1,
		.param2.U = 0,
		NULL, mc_cmd_play, 0, 0 },

{ 20, 27, "R", "<R>ewind", MI_HOTKEY,
		.param1.U = 1,
		.param2.U = 0,
		NULL, mc_cmd_rew, 0, 0 },

{ 20, 36, "F", "<F>orward", MI_HOTKEY,
		.param1.U = 1,
		.param2.U = 0,
		NULL, mc_cmd_fwd, 0, 0 },

{ PROMPT_ROW, PROMPT_COL, "", "Option: ", MI_PROMPT,
		.param1.U = 0,
		.param2.U = 0,
		NULL, NULL, 0, 0 }
};

/* MAIN MENU */

MENU menu_main = { 
    MENU_MAIN, 
    main_items, 
    sizeof(main_items) / sizeof(MENUITEM),
    "MAIN MENU"
};

/* End-Of-File */
