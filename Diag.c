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

/* Project specific includes */
#include "DTC1200.h"
#include "PID.h"
#include "Globals.h"
//#include "Utils.h"
#include "ServoTask.h"
#include "TransportTask.h"
#include "TerminalTask.h"
#include "Diag.h"

#include "IOExpander.h"
#include "tty.h"

/* Static Data Items */

typedef struct _BITTAB {
    unsigned char  bit;
    char    name[10];
} BITTAB;

static const BITTAB s_lamp[] = {
    { L_STOP,   	"stop", },
    { L_FWD,    	"fwd ", },
    { L_REW,    	"rew ", },
    { L_PLAY,   	"play", },
    { L_REC,    	"rec ", },
};

static const char s_crlf[]     = { "\r\n" };
static const char s_startstr[] = { "%s - <ESC> aborts\r\n\n" };
static const char s_waitstr[]  = { "\r\nAny key continues..." };

/* Static Function Prototypes */

static int wait4continue(void);

/*
 * Diagnostic static helper functions.
 */

static int wait4continue(void)
{
	int key;

    tty_puts(s_waitstr);

    while(1)
    {
 		if (tty_getc(&key) != 0)
    		break;
    }

    return key;
}

static bool check_halt()
{
	if ((!Servo_IsMode(MODE_HALT)) || (Servo_IsMotion()))
	{
		tty_printf("%sWARNING%s - Transport must be in HALT mode with no tape/reels mounted!!\r\n",
				   VT100_UL_ON, VT100_UL_OFF);
		return false;
	}

	return true;
}

//*****************************************************************************
// DIAGNOSTIC FUNCTIONS
//*****************************************************************************

/*
 * This set the DAC's to various zero torque reference points
 * on the takeup and supply reel motors.
 */
 
int diag_dac_adjust(MENUITEM* mp)
{
	int ch;

    tty_cls();
    tty_printf(s_startstr, mp->menutext);

    if (!check_halt())
    {
        wait4continue();
    }
    else
    {
    	int i=0;
    	static uint32_t dac[] = { 0, 50, 75, 100, 125, 150, 175, 200, 225, 250 };

        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);

        /* Release the brakes */
        SetTransportMask(0, T_BRAKE);

        while (1)
        {
            tty_printf("DAC level: %-4u (<ESC> or 'N'=next, 'P'=prev)\r\n", dac[i]);

            g_servo.dac_halt_takeup = g_servo.dac_halt_supply = dac[i];

            while (tty_getc(&ch) == 0);

            ch = toupper(ch);

            if ((ch == ESC) || (ch == 'X'))
            	break;

            if ((ch == 'N') || (ch == ' '))
            {
            	if (i < (sizeof(dac)/sizeof(uint32_t)) - 1)
            		++i;
            }
            else if (ch == 'P')
            {
            	if (i > 0)
            		--i;
            }
        }

        g_servo.dac_halt_takeup = g_servo.dac_halt_supply = DAC_MIN;
        SetTransportMask(T_BRAKE, 0);

        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);
    }
    
    return 1;
}

/*
 * This ramps the DAC's on the takeup and supply reel motors.
 */
 
int diag_dac_ramp(MENUITEM* mp)
{
	int ch;
	long dac = DAC_MIN;

    tty_cls();
    tty_printf(s_startstr, mp->menutext);

    if (check_halt())
    {
        tty_printf("%sWARNING%s - Stay clear as reels can run up to full speed!!\r\n\n",
                   VT100_UL_ON, VT100_UL_OFF);
        
        /* Transport MUST be in halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);

        /* Release the brakes */
        SetTransportMask(0, T_BRAKE);

        while(1)
        {
            tty_printf("DAC A/B level: %-4.4u (<ESC> or 'u'=up, 'd'=down)\r\n", dac);

            g_servo.dac_halt_takeup = (unsigned long)dac;
            g_servo.dac_halt_supply = (unsigned long)dac;

            if (dac >= DAC_MAX)
            	dac = 0;

            while (tty_getc(&ch) == 0);

            if ((ch == ESC) || (toupper(ch) == 'X'))
            	break;

            if (ch == 'u')
            	++dac;
            else if (ch == 'U')
            	dac += 10;
            else if (ch == 'd')
            	--dac;
            else if (ch == 'D')
            	dac -= 10;
            else
            	++dac;

            /* Range Check */
            if (dac > DAC_MAX)
            	dac = DAC_MAX;
            else if (dac < 0)
            	dac = 0;
        }

        /* Reset the global halt DAC levels */
        g_servo.dac_halt_takeup = g_servo.dac_halt_supply = DAC_MIN;
        SetTransportMask(T_BRAKE, 0);

        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);
    }
    
    wait4continue();

    return 1;
}

/*
 * This routine tests the transport pinch roller solenoid
 */

int diag_pinch_roller(MENUITEM* mp)
{
	int ch;
	int state = 0;

    tty_cls();
    tty_printf(s_startstr, mp->menutext);

    if (check_halt())
    {
        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);

        /* Release pinch roller */
        SetTransportMask(0, T_PROL);

        while (1)
        {
            tty_printf("Pinch Roller: %s (<ESC> exit, SPACE=toggle)\r\n", state ? "ON " : "OFF");

        	/* Wait for a keystroke */
            while (tty_getc(&ch) == 0);

           	if ((ch == ESC) || (toupper(ch) == 'X'))
           		break;

        	state ^= 1;

			/* Release pinch roller */
			if (state)
				SetTransportMask(T_PROL, 0);
			else
				SetTransportMask(0, T_PROL);

        }

        /* Release pinch roller */
        SetTransportMask(0, T_PROL);

        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);
    }

    return 1;
}

/*
 * This routine tests the transport brakes solenoid
 */

int diag_brakes(MENUITEM* mp)
{
    int ch;
    int state = 1;

    tty_cls();
    tty_printf(s_startstr, mp->menutext);

    if (check_halt())
    {
        /* Transport back to halt mode */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);

        /* Engage brakes */
        SetTransportMask(T_BRAKE, 0);

        while (1)
        {
            tty_printf("Reel Motor Brakes: %s (<ESC> exit, SPACE=toggle)\r\n", state ? "ON " : "OFF");

            /* Wait for a keystroke */
            while (tty_getc(&ch) == 0);

            if ((ch == ESC) || (toupper(ch) == 'X'))
                break;

            state ^= 1;

            /* Set new brake state */
            if (state)
                SetTransportMask(T_BRAKE, 0);
            else
                SetTransportMask(0, T_BRAKE);

        }

        /* Engage brakes */
        SetTransportMask(T_BRAKE, 0);

        /* Transport back to halt mode */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);
    }

    return 1;
}

/*
 * This routine tests the transport tape lifters solenoid
 */

int diag_lifters(MENUITEM* mp)
{
    int ch;
    int state = 0;

    tty_cls();
    tty_printf(s_startstr, mp->menutext);

    if (check_halt())
    {
        /* Transport back to halt mode */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);

        /* Release lifters */
        SetTransportMask(0, T_TLIFT);

        while (1)
        {
            tty_printf("Tape Lifters: %s (<ESC> exit, SPACE=toggle)\r\n", state ? "ON " : "OFF");

            /* Wait for a keystroke */
            while (tty_getc(&ch) == 0);

            if ((ch == ESC) || (toupper(ch) == 'X'))
                break;

            state ^= 1;

            /* Set pinch roller state */
            if (state)
                SetTransportMask(T_TLIFT, 0);
            else
                SetTransportMask(0, T_TLIFT);
        }

        /* Release Lifters */
        SetTransportMask(0, T_TLIFT);

        /* Transport back to halt mode */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);
    }

    return 1;
}

/*
 * This routine tests the transport tape lifters solenoid
 */

int diag_servo(MENUITEM* mp)
{
    int ch;
    int state = 0;

    tty_cls();
    tty_printf(s_startstr, mp->menutext);

    if (check_halt())
    {
        /* Transport back to halt mode */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);

        /* Capstan servo off */
        SetTransportMask(0, T_SERVO);

        while (1)
        {
            tty_printf("Capstan Servo Motor: %s (<ESC> exit, SPACE=toggle)\r\n", state ? "ON " : "OFF");

            /* Wait for a keystroke */
            while (tty_getc(&ch) == 0);

            if ((ch == ESC) || (toupper(ch) == 'X'))
                break;

            state ^= 1;

            /* toggle capstan servo motor */
            if (state)
                SetTransportMask(T_SERVO, 0);
            else
                SetTransportMask(0, T_SERVO);

        }

        /* Capstan servo off */
        SetTransportMask(0, T_SERVO);

        /* Transport back to halt mode */
        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT, 0);
    }

    return 1;
}

/*
 * This routine blinks the lamps sequentially for testing.
 */
 
int diag_lamp(MENUITEM* mp)
{
    int i, ch;
    int loop = 1;
    unsigned char save_mask;
    
    tty_cls();
    tty_printf(s_startstr, mp->menutext);

    save_mask = g_lamp_mask;
    
    while(loop)
    {
        for (i=0; i < sizeof(s_lamp)/sizeof(BITTAB); i++)
        {
            tty_printf("\rlamp: %s", s_lamp[i].name);
        
            g_lamp_mask |= s_lamp[i].bit;

            SetLamp(g_lamp_mask);

            if (tty_getc(&ch))
            {
            	if ((ch == ESC) || (toupper(ch) == 'X'))
            	{
                    loop = 0;
            		break;
            	}
            }

            g_lamp_mask &= ~(s_lamp[i].bit);
        }

        tty_puts(s_crlf);
    }

    g_lamp_mask = save_mask;
    
    wait4continue();

    return 1;
}

#if (CAPDATA_SIZE > 0)
int diag_dump_capture(MENUITEM* mp)
{
	long i;

    tty_cls();
    tty_printf("Capture Data (Count=%d)\r\n", g_capdata_count);
    tty_printf("Sample, DAC-Sup, DAC-Tkup, VelSup, VelTkup, RadSup, RadTkup, Tach, Tens\r\n");

    for (i=0; i < CAPDATA_SIZE; i++)
    {
    	tty_printf("%u,%u,%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\r\n",
   			i,
    		(uint32_t)g_capdata[i].dac_supply,
    		(uint32_t)g_capdata[i].dac_takeup,
    		g_capdata[i].vel_supply,
    		g_capdata[i].vel_takeup,
    		g_capdata[i].rad_supply,
    		g_capdata[i].rad_takeup,
    		g_capdata[i].tape_tach,
    		g_capdata[i].tension);
    }

    wait4continue();

    return 1;
}
#endif

/* end-of-file */
