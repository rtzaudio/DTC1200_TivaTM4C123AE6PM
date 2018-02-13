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
 * STOP MENU ITEMS
 *****************************************************************************/

static MENUITEM stop_items[] = {

{ 3, 6, "", "STOP SERVO", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{  5, 2, "1", "Stop Brake Torque", MI_LONG,
		.param1.U = 300,
		.param2.U = 900,
		NULL, set_idata, DT_LONG, &g_sys.stop_brake_torque },

{  7, 6, "", "STOP SETTINGS", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{  9, 2,  "2", "Lifter Engaged at STOP", MI_BITFLAG,
		.param1.U = SF_LIFTER_AT_STOP,
		.param2.U = SF_LIFTER_AT_STOP,
        NULL, NULL, DT_LONG, &g_sys.sysflags },

{ 10, 2, "3", "Brakes Engaged at STOP", MI_BITFLAG,
		.param1.U = SF_BRAKES_AT_STOP,
		.param2.U = SF_BRAKES_AT_STOP,
        NULL, NULL, DT_LONG, &g_sys.sysflags },

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT,
		.param1.U = 0,
		.param2.U = 0,
		NULL, NULL, 0, 0 }
};

/** STOP SERVO MENU **/

MENU menu_stop = {
    MENU_STOP,
    stop_items,
    sizeof(stop_items) / sizeof(MENUITEM),
    "STOP MENU"
};

/* End-Of-File */
