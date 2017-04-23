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

extern uint8_t g_lamp_mask;
extern uint8_t g_lamp_mask_prev;
extern uint8_t g_lamp_blink_mask;

extern uint8_t g_dip_switch;
extern uint8_t g_high_speed_flag;
extern uint8_t g_tape_out_flag;
extern uint32_t g_tape_width;		/* tape width 0=one-inch, 1=two-inch */

extern uint8_t g_ui8SerialNumber[16];

#endif // __GLOBALS_H__
