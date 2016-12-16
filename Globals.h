//*****************************************************************************
//
// globals.h - Shared configuration and global variables.
//
// Copyright (c) 2006-2011 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 7243 of the EK-LM3S8962 Firmware Package.
//
//*****************************************************************************

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

/* Handles created dynamically */
extern Mailbox_Handle g_mailboxCommander;
extern Mailbox_Handle g_mailboxController;

extern I2C_Handle g_handleI2C0;
extern I2C_Handle g_handleI2C1;

extern UART_Handle g_handleUartTTY;
extern UART_Handle g_handleUartIPC;

// Servo Capture Data
#if (CAPDATA_SIZE > 0)
extern CAPDATA g_capdata[CAPDATA_SIZE];
extern long g_capdata_count;
#endif

/*** Global Data Items *****************************************************/

extern SYSPARMS  g_sys;
extern SERVODATA g_servo;

extern long g_cmode;

extern uint8_t g_lamp_mask;
extern uint8_t g_lamp_mask_prev;
extern uint8_t g_lamp_blink_mask;

extern uint8_t g_switch_option;
extern uint8_t g_high_speed_flag;
extern uint8_t g_tape_out_flag;

#endif // __GLOBALS_H__
