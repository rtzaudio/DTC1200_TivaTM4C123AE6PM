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
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* Tivaware Driver files */
#include <driverlib/eeprom.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* XDCtools Header files */
#include "Board.h"
#include "DTC1200.h"
#include "Globals.h"
#include "TransportTask.h"
#include "ServoTask.h"
#include "TerminalTask.h"
#include "ServoTask.h"
#include "TachTimer.h"
#include "MotorDAC.h"
#include "IOExpander.h"
#include "Diag.h"

Semaphore_Handle g_semaSPI;


/* Static Function Prototypes */
Int main();
Void MainPollTask(UArg a0, UArg a1);

//*****************************************************************************
// Main Program Entry Point
//*****************************************************************************

Int main()
{
    Error_Block eb;
    Mailbox_Params mboxParams;
    Task_Params taskParams;

    /* Call board init functions */
    Board_initGeneral();
	Board_initGPIO();
	Board_initUART();
	Board_initI2C();
	Board_initSPI();
	Board_initQEI();
	Board_initADC();

    /*
     * Create a mailbox message queues for the transport control/command tasks
     */

	/* Create transport controller task mailbox */

    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    if ((g_mailboxCommander = Mailbox_create(sizeof(uint8_t), 8, &mboxParams, &eb)) == NULL)
    {
        System_abort("Mailbox_create() failed!\n");
    }

    /* Create servo controller task mailbox */

    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    if ((g_mailboxController = Mailbox_create(sizeof(CMDMSG), 8, &mboxParams, &eb)) == NULL)
    {
        System_abort("Mailbox_create() failed!\n");
    }

    /* Create a global binary semaphore for serialized access to the SPI bus */
    //Semaphore_Params_init(&semaParams);
    //semaParams.mode =
    if ((g_semaSPI = Semaphore_create(1, NULL, &eb)) == NULL)
    {
        System_abort("Semaphore_create() failed!\n");
    }

    /*
     * Now start the main application button polling task
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 2;

    if (Task_create(MainPollTask, &taskParams, &eb) == NULL)
    {
        System_abort("MainPollTask create failed!\n");
    }

    BIOS_start();    /* does not return */

    return(0);
}

//*****************************************************************************
// Initialize and open various system peripherals we'll be using
//*****************************************************************************

void InitPeripherals(void)
{
	I2C_Params i2cParams;

	/*
	 * Open the I2C ports for peripherals we need to communicate with.
	 */

	/* Open I2C0 for U14 CAT24C08 EPROM */

	I2C_Params_init(&i2cParams);
	i2cParams.transferMode	= I2C_MODE_BLOCKING;
	i2cParams.bitRate       = I2C_100kHz;
	g_handleI2C0 = I2C_open(Board_I2C0, &i2cParams);

	/* Open I2C1 for U9 AT24CS01 EPROM/Serial# */

	I2C_Params_init(&i2cParams);
	i2cParams.transferMode	= I2C_MODE_BLOCKING;
	i2cParams.bitRate       = I2C_100kHz;
	g_handleI2C1 = I2C_open(Board_I2C1, &i2cParams);

	/*
     * Open the SPI ports for peripherals we need to communicate with.
     */

	/* Initialize the SPI-0 to the reel motor DAC's */
	MotorDAC_initialize();

    /* Initialize the SPI-1 & SPI-2 to the I/O expanders */
	IOExpander_initialize();
}

//*****************************************************************************
//
//*****************************************************************************

Void MainPollTask(UArg a0, UArg a1)
{
	uint8_t count = 0;
    uint8_t bits = 0x00;
    uint8_t tran_prev = 0xff;
    uint8_t mode_prev = 0xff;
    uint8_t tout_prev = 0xff;
	uint8_t debounce_tran = 0;
	uint8_t debounce_mode = 0;
	uint8_t debounce_tout = 0;
    Error_Block eb;
    Task_Params taskParams;

    System_printf("MainPollTask()\n");

	/* Initialize the default servo and program data values */

	memset(&g_servo, 0, sizeof(SERVODATA));
    memset(&g_sys, 0, sizeof(SYSPARMS));

    InitSysDefaults(&g_sys);

    InitPeripherals();

    /* Read the initial mode switch states */
	GetModeSwitches(&mode_prev);
    g_high_speed_flag = (mode_prev & M_HISPEED) ? 1 : 0;
	g_switch_option   = mode_prev & M_DIPSW_MASK;

    /* Read the initial transport switch states */
	GetTransportSwitches(&tran_prev);
   	g_tape_out_flag   = (tran_prev & S_TAPEOUT) ? 1 : 0;

    /* Read the system config parameters from storage */
    if (SysParamsRead(&g_sys) != 0) {
    	/* blink ALL lamps on error */
    	LampBlinkError();
    } else {
		/* blink each lamp individually on success */
    	LampBlinkChase();
    }

    /* Initialize servo loop controller data */
    g_servo.offset_sample_cnt = 0;
    g_servo.offset_null_sum   = 0;
	g_servo.tsense_sum        = 0;
	g_servo.tsense_sample_cnt = 0;
	g_servo.brake_torque      = 0;
    g_servo.dac_halt_takeup   = 0;
    g_servo.dac_halt_supply   = 0;

    /* Servo's start in halt mode! */
    SET_SERVO_MODE(MODE_HALT);

    /* Create the transport command/control and terminal tasks.
     * The three tasks provide the bulk of the system control services.
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 9;
    if (Task_create(TransportCommandTask, &taskParams, &eb) == NULL)
    {
        System_abort("Transport command task create failed!\n");
    }

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 10;
    if (Task_create(TransportControllerTask, &taskParams, &eb) == NULL)
    {
        System_abort("Transport controller task create failed!\n");
    }

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 2048;
    taskParams.priority  = 1;
    if (Task_create(TerminalTask, &taskParams, &eb) == NULL)
    {
        System_abort("Terminal task create failed!\n");
    }

    /* Initialize the tape tach timers and interrupt.
     * This tach provides the tape speed from the tape
     * counter roller.
     */

    Tachometer_initialize();

    /* Start the transport servo loop timer clock running.
     * We create an auto-reload periodic timer for the servo loop.
     * Note we are servicing the servo task at 500 Hz.
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 500;
    taskParams.priority  = 15;

    if (Task_create(ServoLoopTask, &taskParams, &eb) == NULL)
    {
        System_abort("Servo loop task create failed!\n");
    }

    /****************************************************************
     * Enter the main application button processing loop forever.
     ****************************************************************/

    g_lamp_blink_mask = L_STAT1;

	for(;;)
	{
		/* Blink heartbeat LED1 on the transport interface card */
		if (++count >= 50)
		{
			count = 0;
			g_lamp_mask ^= g_lamp_blink_mask;
		}

        /* Set any new led/lamp state. We only update the LED output
         * port if a new lamp state was selected.
         */

        if (g_lamp_mask != g_lamp_mask_prev)
        {
            // Set the new lamp state
            SetLamp(g_lamp_mask);

            // Upate the previous lamp state
            g_lamp_mask_prev = g_lamp_mask;
        }

		/*
		 * Poll the transport control buttons to read the
		 * current transport button switch states
		 */

   		GetTransportSwitches(&bits);

   		/* First process the tape out arm switch */
   		if ((bits & S_TAPEOUT) != tout_prev)
   		{
   			if (++debounce_tout >= 2)
   			{
   				debounce_tout = 0;

   				/* Save the new state */
            	tout_prev = (bits & S_TAPEOUT);

   				/* Set the tape out arm state */
            	g_tape_out_flag = (bits & S_TAPEOUT) ? 1 : 0;

            	if (!(bits & S_TAPEOUT))
            		bits |= S_TAPEIN;

            	/* Send the button press to transport ctrl/cmd task */
            	Mailbox_post(g_mailboxCommander, &bits, 10);
   			}
   		}

   		/* Next process the tape transport buttons */
   		bits &= S_BUTTON_MASK;

   		if (bits != tran_prev)
   		{
   			if (++debounce_tran >= 2)
   			{
   				debounce_tran = 0;

   				/* Debounced a button press, send it to transport task */
   				tran_prev = bits;

            	/* Send the button press to transport ctrl/cmd task */
            	Mailbox_post(g_mailboxCommander, &bits, 10);
    		}
    	}

        /*
         * Poll the mode config switches to read the DIP switches on
         * the PCB and the hi/lo speed switch on the transport control.
         */

        GetModeSwitches(&bits);

        if (bits != mode_prev)
        {
   			if (++debounce_mode >= 2)
   			{
   				debounce_mode = 0;

   				/* We debounced a switch change, reset and process it */
   				mode_prev = bits;

            	/* Save the transport speed select setting */
				g_high_speed_flag = (bits & M_HISPEED) ? 1 : 0;

				/* Save the updated DIP switch settings */
				g_switch_option = bits & M_DIPSW_MASK;
   			}
        }

        /* delay for 10ms and loop */
        Task_sleep(10);
	}
}

//*****************************************************************************
// Set default runtime values
//*****************************************************************************

void InitSysDefaults(SYSPARMS* p)
{
    /* default servo parameters */
    p->version                  = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    p->debug                    = 0;    	/* debug mode 0=off 				*/
#if (QE_TIMER_PERIOD > 500000)
    p->shuttle_slow_velocity    = 0;	    /* reduce velocity to speed         */
    p->shuttle_slow_offset      = 110;	    /* offset to reduce velocity at     */
#else
    p->shuttle_slow_velocity    = 0;	    /* reduce velocity to speed         */
    p->shuttle_slow_offset      = 60;	    /* offset to reduce velocity at     */
#endif
    p->lifter_settle_time       = 800; 		/* tape lifter settling delay in ms */
    p->pinch_settle_time        = 250;	    /* start 250ms after pinch roller   */
    p->brakes_stop_play         = 1;        /* brakes stop from play mode if 1  */
    p->engage_pinch_roller		= 1;		/* engage pinch roller during play  */
    p->record_pulse_length      = REC_PULSE_DURATION;
    p->tension_sensor_gain      = 2;

#if (QE_TIMER_PERIOD > 500000)
    p->velocity_detect          = 100;      /* 100 pulses or less = no velocity */
    p->null_offset_gain         = 2;	    /* null offset gain */
#else
    p->velocity_detect          = 5;        /* 5 pulses or less = no velocity  */
    p->null_offset_gain         = 3;    	/* null offset gain */
#endif
    p->stop_supply_tension      = 220;      /* supply tension level (0-DAC_MAX) */
    p->stop_takeup_tension      = 220;      /* takeup tension level (0-DAC_MAX) */
    p->stop_max_torque          = DAC_MAX;  /* max stop servo torque (0-DAC_MAX)*/
    p->stop_min_torque          = 10;		/* min stop servo torque            */
#if (QE_TIMER_PERIOD > 500000)
    p->stop_brake_torque        = 450;      /* max stop brake torque            */
#else
    p->stop_brake_torque        = 300;      /* max stop brake torque            */
#endif

    p->shuttle_supply_tension   = 220;		/* shuttle supply reel tension      */
    p->shuttle_takeup_tension   = 220;		/* shuttle takeup reel tension      */
    p->shuttle_max_torque       = DAC_MAX;  /* shuttle max torque 				*/
    p->shuttle_min_torque       = 10;		/* shuttle min torque 				*/
#if (QE_TIMER_PERIOD > 500000)
    p->shuttle_velocity         = 3500;     /* max shuttle velocity 			*/
    p->shuttle_servo_pgain      = 32;       /* shuttle mode servo P-gain 		*/
    p->shuttle_servo_igain      = 16;       /* shuttle mode servo I-gain 		*/
    p->shuttle_servo_dgain      = 3;     	/* shuttle mode servo D-gain 		*/
#else
    p->shuttle_velocity         = 350;      /* max shuttle velocity 			*/
    p->shuttle_servo_pgain      = 32;     	/* shuttle mode servo P-gain 		*/
    p->shuttle_servo_igain      = 16;       /* shuttle mode servo I-gain 		*/
    p->shuttle_servo_dgain      = 3;       	/* shuttle mode servo D-gain 		*/
#endif

    p->play_tension_gain		= 10;		/* play tension velocity gain factor*/
    p->play_lo_supply_tension   = 220;      /* supply tension level (0-DAC_MAX) */
    p->play_lo_takeup_tension   = 220;      /* takeup tension level (0-DAC_MAX) */
    p->play_hi_supply_tension   = 220;      /* supply tension level (0-DAC_MAX) */
    p->play_hi_takeup_tension   = 220;      /* takeup tension level (0-DAC_MAX) */
    p->play_max_torque          = DAC_MAX;  /* play mode max torque (0-DAC_MAX)	*/
    p->play_min_torque          = 10;		/* play mode min torque (0-DAC_MAX) */
    p->play_lo_boost_time       = 1500;     /* play mode accel boost from stop  */
    p->play_lo_boost_step		= 10;
    p->play_hi_boost_time       = 3000;     /* play mode accel boost from stop  */
    p->play_hi_boost_step		= 8;
    p->play_lo_boost_start      = DAC_MAX;
    p->play_lo_boost_end        = 220;
    p->play_hi_boost_start      = DAC_MAX;
    p->play_hi_boost_end        = 220;

    p->reserved3                = 0;        /* reserved */
    p->reserved4                = 0;        /* reserved */
}

//*****************************************************************************
// Write system parameters from our global settings buffer to EEPROM.
//
// Returns:  0 = Sucess
//          -1 = Error writing EEPROM data
//*****************************************************************************

int SysParamsWrite(SYSPARMS* sp)
{
	int32_t rc = 0;

	sp->version = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
	sp->magic   = MAGIC;

	rc = EEPROMProgram((uint32_t *)sp, 0, sizeof(SYSPARMS));

    return rc;
 }

//*****************************************************************************
// Read system parameters into our global settings buffer from EEPROM.
//
// Returns:  0 = Sucess
//          -1 = Error reading flash
//
//*****************************************************************************

int SysParamsRead(SYSPARMS* sp)
{
    InitSysDefaults(sp);

    EEPROMRead((uint32_t *)sp, 0, sizeof(SYSPARMS));

    if (sp->magic != MAGIC)
    {
        System_printf("ERROR Reading System Parameters - Using Defaults...\n");
        System_flush();
    	InitSysDefaults(sp);
    	return -1;
    }

    if (sp->version != MAKEREV(FIRMWARE_VER, FIRMWARE_REV))
    {
    	System_printf("WARNING New Firmware Version - Using Defaults...\n");
        System_flush();
    	InitSysDefaults(sp);
    }

	return 0;
}
