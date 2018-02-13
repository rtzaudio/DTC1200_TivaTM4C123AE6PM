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
 * DIAG MENU ITEMS
 *****************************************************************************/

static MENUITEM diag_items[] = {

{  3, 6, NULL, "DIAGNOSTICS", MI_TEXT,
		.param1.U = 1,
		.param2.U = 0,
		NULL, NULL, 0, 0 },

{  5, 2, "1", "Lamp Test", MI_EXEC,
		.param1.U = 0,
		.param2.U = 1,
		NULL, diag_lamp, 0, 0 },

{  6, 2, "2", "Transport Test", MI_EXEC,
		.param1.U = 0,
		.param2.U = 1,
		NULL, diag_transport, 0, 0 },

{  7, 2, "3", "Pinch Roller Engage", MI_EXEC,
		.param1.U = 0,
		.param2.U = 1,
		NULL, diag_pinch_roller, 0, 0 },

{  8, 2, "4", "MDA DAC Ramp Test", MI_EXEC,
		.param1.U = 0,
		.param2.U = 1,
		NULL, diag_dacramp, 0, 0 },

{  9, 2, "5", "MDA DAC Zero Trim", MI_EXEC,
		.param1.U = 0,
		.param2.U = 1,
		NULL, diag_dacadjust, 0, 0 },

#if (CAPDATA_SIZE > 0)
{ 10, 2, "6", "Dump Capture Data", MI_EXEC,
		.param1.U = 0,
		.param2.U = 1,
		NULL, diag_dump_capture, 0, 0 },
#endif

{ PROMPT_ROW, PROMPT_COL, "", "", MI_PROMPT,
		.param1.U = 0,
		.param2.U = 1,
		NULL, NULL, 0, 0 }
};

/** DIAGNOSTICS MENU **/

MENU menu_diag = {
    MENU_DIAG,
    diag_items,
    sizeof(diag_items) / sizeof(MENUITEM),
    "DIAGNOSTIC MENU"
};

/* End-Of-File */
