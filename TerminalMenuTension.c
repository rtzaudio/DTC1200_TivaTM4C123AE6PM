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
 * TENSION MENU ITEMS
 *****************************************************************************/

#define MAX_TENSION     (DAC_MAX)

static MENUITEM tension_items[] = {

{ 3, 6, "", "SUPPLY TENSION", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 2, "1", "Stop   ", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = MAX_TENSION,
		NULL, put_idata, DT_LONG, &g_sys.stop_supply_tension },

{ 6, 2, "2", "Shuttle", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = MAX_TENSION,
		NULL, put_idata, DT_LONG, &g_sys.shuttle_supply_tension },

{ 7, 2, "3", "Play LO", MI_NUMERIC,
		.param1.U = 10,
		.param2.U = 1024,
		NULL, put_idata, DT_LONG, &g_sys.play_lo_supply_tension },

{ 8, 2, "4", "Play HI", MI_NUMERIC,
		.param1.U = 10,
		.param2.U = 1024,
		NULL, put_idata, DT_LONG, &g_sys.play_hi_supply_tension },

{ 3, 30, "", "TAKEUP TENSION", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 26, "5", "Stop   ", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = MAX_TENSION,
		NULL, put_idata, DT_LONG, &g_sys.stop_takeup_tension },

{ 6, 26, "6", "Shuttle", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = MAX_TENSION,
		NULL, put_idata, DT_LONG, &g_sys.shuttle_takeup_tension },

{ 7, 26, "7", "Play LO", MI_NUMERIC,
		.param1.U = 10.0f,
		.param2.U = 1024.0f,
		NULL, put_idata, DT_LONG, &g_sys.play_lo_takeup_tension },

{ 8, 26, "8", "Play HI", MI_NUMERIC,
		.param1.U = 10.0f,
		.param2.U = 1024.0f,
		NULL, put_idata, DT_LONG, &g_sys.play_hi_takeup_tension },

#if 0
{ 10, 6, "", "MIN TORQUE", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 12, 2, "9", "Stop   ", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = MAX_TENSION,
		NULL, put_idata, DT_LONG, &g_sys.stop_min_torque },

{ 13, 2, "10", "Shuttle", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = MAX_TENSION,
		NULL, put_idata, DT_LONG, &g_sys.shuttle_min_torque },

{ 14, 2, "11", "Play   ", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = MAX_TENSION,
		NULL, put_idata, DT_LONG, &g_sys.play_min_torque },

{ 10, 30, "", "MAX TORQUE", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 12, 26, "12", "Stop   ", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = DAC_MAX,
		NULL, put_idata, DT_LONG, &g_sys.stop_max_torque },

{ 13, 26, "13", "Shuttle", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = DAC_MAX,
		NULL, put_idata, DT_LONG, &g_sys.shuttle_max_torque },

{ 14, 26, "14", "Play   ", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = DAC_MAX,
		NULL, put_idata, DT_LONG, &g_sys.play_max_torque },
#endif

{ 16,  6, "", "SERVO PARAMETERS", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 18,  2, "15", "Reel Offset Gain   ", MI_NUMERIC,
		.param1.F = 0.01f,
		.param2.F = 1.00f,
		NULL, put_idata, DT_FLOAT, &g_sys.reel_offset_gain },

{ 19,  2, "16", "Play Radius Gain   ", MI_NUMERIC,
		.param1.F = 0.025f,
		.param2.F = 1.00f,
		NULL, put_idata, DT_FLOAT, &g_sys.play_radius_gain },

{ 20,  2, "17", "Tension Sensor Gain", MI_NUMERIC,
		.param1.F = 0.0f,
		.param2.F = 1.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.tension_sensor_gain },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT,
		.param1.U = 0,
		.param2.U = 0,
		NULL, NULL, 0, 0 }
};

/** TENSION MENU **/

MENU menu_tension = {
    MENU_TENSION,
    tension_items,
    sizeof(tension_items) / sizeof(MENUITEM),
    "TENSION MENU"
};

/* End-Of-File */
