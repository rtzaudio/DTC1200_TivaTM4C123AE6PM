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

uint8_t g_lamp_mask       = 0;        	/* current led/lamp output mask */
uint8_t g_lamp_mask_prev  = 0xff;     	/* prev led/lamp output mask    */
uint8_t g_lamp_blink_mask = 0;

uint8_t g_dip_switch      = 0;        	/* option DIP switch settings */
uint8_t g_high_speed_flag = 0;        	/* non-zero if hi-speed tape  */
uint8_t g_tape_out_flag   = 0;        	/* current tape out status */

uint8_t g_ui8SerialNumber[16];

uint32_t g_tape_width;					/* tape width 0=one-inch, 1=two-inch */

#if (CAPDATA_SIZE > 0)
CAPDATA g_capdata[CAPDATA_SIZE];
long g_capdata_count = 0;
#endif

/* end-of-file */
