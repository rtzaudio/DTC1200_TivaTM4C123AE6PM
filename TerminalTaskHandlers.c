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
#include "tty.h"

/* Global Data Access */

extern const char g_ul_on[];
extern const char g_ul_off[];
extern const char g_inv_on[];
extern const char g_inv_off[];
extern const char g_escstr[];
extern const char g_title[];

/*****************************************************************************
 * EXECUTE MENU COMMAND HANDLERS
 *****************************************************************************/

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
    tty_printf(g_title, FIRMWARE_VER, FIRMWARE_REV);

    if (g_sys.debug == 1)
    {
        tty_pos(2, 64);
        tty_printf("%sMONITOR%s", g_inv_on, g_inv_off);

        tty_pos(3, 2);
        tty_printf("%sSUPPLY REEL%s", g_ul_on, g_ul_off);
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
        tty_printf("%sTAKEUP REEL%s", g_ul_on, g_ul_off);
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
        tty_printf("%sSHUTTLE PID%s", g_ul_on, g_ul_off);
        tty_pos(12, 2);
        tty_puts("PID CV");
        tty_pos(13, 2);
        tty_puts("PID Error");
        tty_pos(14, 2);
        tty_puts("Target");
        tty_pos(15, 2);
        tty_puts("Velocity");
        tty_pos(16, 2);
        tty_puts("Hold Back");

        tty_pos(18, 2);
        tty_printf("%sTAPE%s", g_ul_on, g_ul_off);
        tty_pos(19, 2);
        tty_printf("Tape Tach");
        tty_pos(20, 2);
        tty_printf("Tension Arm");

        tty_pos(12, 35);
        tty_puts("CPU Temp F");

        tty_pos(23, 2);
        tty_puts(g_escstr);
    }
    else
    {
        tty_printf("\r\n\n%s\r\n", g_escstr);
    }
}

static int get_dir_char(void)
{
    int ch;

    if (g_servo.direction == TAPE_DIR_REW) /* rev */
        ch = '<';
    else if (g_servo.direction == TAPE_DIR_FWD) /* fwd */
        ch = '>';
    else
        ch = ' ';

    return ch;
}

void show_monitor_data()
{
    if (g_sys.debug == 1)
    {
    	int ch = get_dir_char();
        tty_pos(4, 27);
        tty_putc(ch);
        tty_putc(ch);

        /* SUPPLY */
        tty_pos(4, 14);
        tty_printf(": %-8u", (uint32_t)g_servo.dac_supply);
        tty_pos(5, 14);
        tty_printf(": %-8.2f", g_servo.velocity_supply);
        tty_pos(6, 14);
        tty_printf(": %-8u", g_servo.qei_supply_error_cnt);
        tty_pos(7, 14);
        tty_printf(": %-8.2f", g_servo.stop_torque_supply);
        tty_pos(8, 14);
        tty_printf(": %-8.2f", g_servo.offset_supply);
        tty_pos(9, 14);
        tty_printf(": %-8.2f", g_servo.radius_supply);

        /* TAKEUP */
        tty_pos(4, 47);
        tty_printf(": %-8u", (uint32_t)g_servo.dac_takeup);
        tty_pos(5, 47);
        tty_printf(": %-8.2f", g_servo.velocity_takeup);
        tty_pos(6, 47);
        tty_printf(": %-8u", g_servo.qei_takeup_error_cnt);
        tty_pos(7, 47);
        tty_printf(": %-8.2f", g_servo.stop_torque_takeup);
        tty_pos(8, 47);
        tty_printf(": %-8.2f", g_servo.offset_takeup);
        tty_pos(9, 47);
        tty_printf(": %-8.2f", g_servo.radius_takeup);

        /* PID SERVO */
        tty_pos(12, 14);
        tty_printf(": %-12.2f", g_servo.db_cv);
        tty_pos(13, 14);
        tty_printf(": %-12.2f", g_servo.db_error);
        tty_pos(14, 14);
        tty_printf(": %-12.2f", g_servo.db_debug);
        tty_pos(15, 14);
        tty_printf(": %-12.2f", g_servo.velocity);
        tty_pos(16, 14);
        tty_printf(": %-12.2f", g_servo.holdback);

        /* TAPE */
        tty_pos(19, 14);
        tty_printf(": %-12.2f", g_servo.tape_tach);
        tty_pos(20, 14);
        tty_printf(": %-8.2f", g_servo.tsense);

        tty_pos(12, 47);
        tty_printf(": %-8.1f", CELCIUS_TO_FAHRENHEIT(ADC_TO_CELCIUS(g_servo.cpu_temp)));
    }
}

/* End-Of-File */
