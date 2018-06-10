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
 * PLAY MENU ITEMS
 *****************************************************************************/

static MENUITEM play_items[] = {

{ 3, 6, NULL, "PLAY BOOST LO", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 2, "1", "Takeup Gain", MI_NUMERIC,
		.param1.F = 1.0f,
		.param2.F = 30.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.play_lo_boost_takeup_gain },

{ 6, 2, "2", "Supply Gain", MI_NUMERIC,
		.param1.F = 1.0f,
		.param2.F = 30.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.play_lo_boost_supply_gain },

{ 7, 2, "3", "Boost End  ", MI_NUMERIC,
		.param1.U = 25,
		.param2.U = 100,
		NULL, put_idata, DT_LONG, &g_sys.play_lo_boost_end },

{ 3, 38, NULL, "PLAY BOOST HI", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 34, "5", "Takeup Gain", MI_NUMERIC,
		.param1.F = 1.0f,
		.param2.F = 30.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.play_hi_boost_takeup_gain },

{ 6, 34, "6", "Supply Gain", MI_NUMERIC,
		.param1.F = 1.0f,
		.param2.F = 30.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.play_hi_boost_supply_gain },

{ 7, 34, "7", "Boost End  ", MI_NUMERIC,
		.param1.U = 100,
		.param2.U = 130,
		NULL, put_idata, DT_LONG, &g_sys.play_hi_boost_end },

{ 11, 6, NULL, "PLAY SETTINGS", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 13, 2, "10", "Pinch Roller Settling Time   ", MI_NUMERIC,
		.param1.U = 0,
		.param2.U = 1000,
		NULL, put_idata, DT_LONG, &g_sys.pinch_settle_time },

{ 14, 2, "11", "Shuttle to Play Settling Time", MI_NUMERIC,
		.param1.U = 0,
		.param2.U = 1000,
		NULL, put_idata, DT_LONG, &g_sys.play_settle_time },

{ 15, 2, "12", "Brake Settle Time            ", MI_NUMERIC,
		.param1.U = 0,
		.param2.U = 2000,
		NULL, put_idata, DT_LONG, &g_sys.brake_settle_time },

{ 16, 2, "13", "Use Brakes to Stop Play Mode ", MI_BITFLAG,
		.param1.U = SF_BRAKES_STOP_PLAY,
		.param2.U = SF_BRAKES_STOP_PLAY,
        NULL, NULL, DT_LONG, &g_sys.sysflags },

{ 17, 2, "14", "Engage Pinch Roller at Play  ", MI_BITFLAG,
		.param1.U = SF_ENGAGE_PINCH_ROLLER,
		.param2.U = SF_ENGAGE_PINCH_ROLLER,
        NULL, NULL, DT_LONG, &g_sys.sysflags },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT,
		0,
		0,
		NULL, NULL, 0, 0 }
};

/** PLAY MENU **/

MENU menu_play = {
    MENU_PLAY,
    play_items,
    sizeof(play_items) / sizeof(MENUITEM),
    "PLAY MENU"
};

/* End-Of-File */
