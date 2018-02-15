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
 * SHUTTLE MENU ITEMS
 *****************************************************************************/

static MENUITEM shuttle_items[] = {

{ 3, 6, "", "SHUTTLE SERVO PID", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 2, "1", "P-Gain", MI_NUMERIC,
		.param1.F = 0.0f,
		.param2.F = 5.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.shuttle_servo_pgain },

{ 6, 2, "2", "I-Gain", MI_NUMERIC,
		.param1.F = 0.0f,
		.param2.F = 2.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.shuttle_servo_igain },

{ 7, 2, "3", "D-Gain", MI_NUMERIC,
		.param1.F = 0.0f,
		.param2.F = 1.0f,
		NULL, put_idata, DT_FLOAT, &g_sys.shuttle_servo_dgain },

{ 9, 6, NULL, "SHUTTLE SETTINGS", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 11, 2, "7", "Shuttle Mode Velocity    ", MI_NUMERIC,
		.param1.U = 50,
		.param2.U = 500,
		NULL, put_idata, DT_LONG, &g_sys.shuttle_velocity },

{ 12, 2, "8", "Auto Decelerate Velocity ", MI_NUMERIC,
		.param1.U = 0,
		.param2.U = 200,
		NULL, put_idata, DT_LONG, &g_sys.shuttle_slow_velocity },

{ 13, 2, "9", "Auto Decelerate at offset", MI_NUMERIC,
		.param1.U = 50,
		.param2.U = 100,
		NULL, put_idata, DT_LONG, &g_sys.shuttle_slow_offset },

{ 14, 2, "10","Lifter Settle Time       ", MI_NUMERIC,
		.param1.U = 0,
		.param2.U = 2000,
		NULL, put_idata, DT_LONG, &g_sys.lifter_settle_time },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT,
		.param1.U = 0,
		.param2.U = 0,
		NULL, NULL, 0, 0 }
};

/** SHUTTLE SERVO MENU **/

MENU menu_shuttle = {
    MENU_SHUTTLE,
    shuttle_items,
    sizeof(shuttle_items) / sizeof(MENUITEM),
    "SHUTTLE MENU"
};

/* End-Of-File */
