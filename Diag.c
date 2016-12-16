/****************************************************************************
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2014, RTZ Professional Audio, LLC
 *
 * The source code contained in this file and associated files is copyright
 * protected material. This material contains the confidential, proprietary
 * and trade secret information of RTZ Professional Audio. No disclosure or
 * use of any portions of this material may be made without the express
 * written consent of RTZ Professional Audio.
 ****************************************************************************/

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

static const BITTAB s_trans[] = {
    { T_SERVO,  	"servo    ", },
    { T_BRAKE,	    "brake off", },
    { T_TLIFT,  	"lifter   ", },
    { T_PROL,   	"roller   ", },
};

static const char s_crlf[]     = { "\r\n" };
static const char s_startstr[] = { "%s - <ESC> aborts test\r\n\n" };
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

/*
 * This function is called at startup to flash all the lamps
 * sequentially to indicate everything is up and running. 
 *
 * NOTE: This can only be called before interrupts are enabled.
 *       The interrupt background routine handles all led/lamp
 *       control once interrupts are enabled.
 */

//*****************************************************************************
// Various Helper and Utility Functions
//*****************************************************************************

void LampBlinkChase(void)
{
    int i;

    static const unsigned char s_lamp[] = { L_STOP, L_FWD, L_REW, L_PLAY, L_REC };

    /* All lamps off */
    SetLamp(0);

    /* Chase the lamps across */
    for (i=0; i < sizeof(s_lamp)/sizeof(unsigned char); i++)
    {
        SetLamp(s_lamp[i]);
        Task_sleep(150);
        SetLamp(0);
    }

    /* All lamps off */
    SetLamp(0);
}

void LampBlinkError(void)
{
    int i;

    /* All lamps off */
    SetLamp(0);

    /* Flash all lamps 3 times on error */
    for (i=0; i < 3; i++)
    {
        SetLamp(L_STOP| L_FWD | L_REW | L_PLAY | L_REC | L_STAT3);
        Task_sleep(200);
        SetLamp(0);
        Task_sleep(100);
    }

    /* All lamps off */
    SetLamp(0);
}

/*
 * This ramps the DAC's on the takeup and supply reel motors.
 */
 
int diag_dacadjust(MENUITEM* mp)
{
    tty_cls();
    tty_printf(s_startstr, mp->text);

    if (!IS_SERVO_MODE(MODE_HALT))
    {
        tty_printf("%sWARNING:%s\r\n\nTransport must be in HALT mode with no tape reels mounted!!\r\n",
                   VT100_UL_ON, VT100_UL_OFF);

        wait4continue();
    }
    else
    {
        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT);

        /* Release the brakes */
        SetTransportMask(0, T_BRAKE);

        tty_printf("DAC levels set to 0V, adjust takeup and supply MDA gain levels for no motion.\r\n");
        tty_printf("Adjust pots until reel motion just begins and trim back till no motion occurs.\r\n\n");
        
        g_servo.dac_halt_takeup = 0;
        g_servo.dac_halt_supply = 0;

        /* Wait for a keystroke */
        wait4continue();

        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT);
    }
    
    return 1;
}

/*
 * This ramps the DAC's on the takeup and supply reel motors.
 */
 
int diag_dacramp(MENUITEM* mp)
{
	int ch;
	long dac = DAC_MIN;

    tty_cls();
    tty_printf(s_startstr, mp->text);

    if ((!IS_SERVO_MODE(MODE_HALT)) || (!IS_STOPPED()))
    {
        tty_printf("%sWARNING%s - Transport must be in HALT mode with no tape/reels mounted!!\r\n",
                   VT100_UL_ON, VT100_UL_OFF);
    }
    else
    {
        tty_printf("%sWARNING%s - Stay clear of reels during test as reels will run up to full speed!!\r\n\n",
                   VT100_UL_ON, VT100_UL_OFF);
        
        /* Transport MUST be in halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT);

        /* Release the brakes */
        SetTransportMask(0, T_BRAKE);

        while(1)
        {
            tty_printf("DAC A/B level: %-4.4u (<ESC> or 'u'=up, 'd'=down)\r\n", dac);

            //tty_printf("%d, %d\r\n", dac, g_servo.adc[0]); //g_servo.tsense);

            g_servo.dac_halt_takeup = (unsigned long)dac;
            g_servo.dac_halt_supply = (unsigned long)dac;

            if (dac >= DAC_MAX)
            	dac = 0;

            while (tty_getc(&ch) == 0);

            if (ch == ESC)
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
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT);
    }
    
    wait4continue();

    return 1;
}

/*
 * This routine test the transport solenoids etc.
 */
 
int diag_transport(MENUITEM* mp)
{
    int i, ch;
    int loop = 1;

    tty_cls();
    tty_printf(s_startstr, mp->text);

    if ((!IS_SERVO_MODE(MODE_HALT)) || (!IS_STOPPED()))
    {
        tty_printf("%sWARNING:%s\r\n\nTransport must be in HALT mode with no tape reels mounted or motion!!\r\n",
                   VT100_UL_ON, VT100_UL_OFF);
    }
    else
    {
        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT);

        SetTransportMask(0, 0xFF);

        while(loop)
        {
            for (i=0; i < sizeof(s_trans)/sizeof(BITTAB); i++)
            {
                tty_printf("\rtransport: %s", s_trans[i].name);
        
                SetTransportMask(s_trans[i].bit, 0);
                Task_sleep(1000);
                SetTransportMask(0, s_trans[i].bit);

                if (tty_getc(&ch))
                {
                	if (ch == ESC)
                	{
                        loop = 0;
                		break;
                	}
                }
            }

            //tty_puts(s_crlf);
        }

        /* Transport back to halt mode */
    	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT);
    }

    wait4continue();

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
    tty_printf(s_startstr, mp->text);

    save_mask = g_lamp_mask;
    
    while(loop)
    {
        for (i=0; i < sizeof(s_lamp)/sizeof(BITTAB); i++)
        {
            tty_printf("\rlamp: %s", s_lamp[i].name);
        
            g_lamp_mask = s_lamp[i].bit;

            SetLamp(g_lamp_mask);

            if (tty_getc(&ch))
            {
            	if (ch == ESC)
            	{
                    loop = 0;
            		break;
            	}
            }

            g_lamp_mask = 0;
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
    	tty_printf("%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
   			i,
    		g_capdata[i].dac_supply,
    		g_capdata[i].dac_takeup,
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
