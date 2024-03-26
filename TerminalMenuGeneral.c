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
 * GENERAL MENU ITEMS
 *****************************************************************************/

static MENUITEM general_items[] = {

{ 3, 6, "", "GENERAL SETTINGS", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{ 5, 2, "1", "Velocity Detect Threshold", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = 50,
		NULL, put_idata, DT_LONG, &g_sys.vel_detect_threshold },

{ 6, 2, "2", "Record Pulse Strobe Time ", MI_NUMERIC,
		.param1.U = 10,
		.param2.U = 100,
		NULL, put_idata, DT_LONG, &g_sys.record_pulse_time },

{ 7, 2, "3", "Record Hold Settle Time  ", MI_NUMERIC,
		.param1.U = 1,
		.param2.U = 10,
		NULL, put_idata, DT_LONG, &g_sys.rechold_settle_time },

{ 8, 2, "4", "Transport Button Debounce", MI_NUMERIC,
		.param1.U = 2,
		.param2.U = 10,
		NULL, put_idata, DT_LONG, &g_sys.debounce },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT,
		.param1.U = 0,
		.param1.U = 0,
		NULL, NULL, 0, 0 }
};

/** GENERAL MENU **/

MENU menu_general = {
    MENU_GENERAL,
    general_items, 
    sizeof(general_items) / sizeof(MENUITEM),
    "GENERAL MENU"
};

/* End-Of-File */
