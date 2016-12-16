/*
 * globals.c : created 7/30/99 10:50:41 AM
 *  
 * Copyright (C) 1999-2000, RTZ Audio. ALL RIGHTS RESERVED.
 *  
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION OF RTZ AUDIO. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF RTZ AUDIO.
 */

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

/* DTC-1200 Specfic Header files */
#include "Board.h"
#include "DTC1200.h"
#include "Globals.h"

/*** GLOBAL DATA ITEMS *****************************************************/

/* Mailbox Handles created dynamically */
Mailbox_Handle g_mailboxCommander = NULL;
Mailbox_Handle g_mailboxController = NULL;

I2C_Handle g_handleI2C0 = NULL;
I2C_Handle g_handleI2C1 = NULL;

UART_Handle g_handleUartTTY = NULL;
UART_Handle g_handleUartIPC = NULL;

/* Runtime and System Parameters */
SYSPARMS g_sys;

/* Servo & System Data in RAM */
SERVODATA g_servo;

long g_cmode = MODE_HALT;    /* current transport mode */

uint8_t g_lamp_mask       = 0;        	/* current led/lamp output mask */
uint8_t g_lamp_mask_prev  = 0xff;     	/* prev led/lamp output mask    */
uint8_t g_lamp_blink_mask = 0;

uint8_t g_switch_option   = 0;        	/* option DIP switch settings */
uint8_t g_high_speed_flag = 0;        	/* non-zero if hi-speed tape  */
uint8_t g_tape_out_flag   = 0;        	/* current tape out status */

#if (CAPDATA_SIZE > 0)
CAPDATA g_capdata[CAPDATA_SIZE];
long g_capdata_count = 0;
#endif

/* end-of-file */
