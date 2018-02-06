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
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* Tivaware Driver files */
#include <driverlib/eeprom.h>
#include <driverlib/fpu.h>

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
#include "IOExpander.h"
#include "TransportTask.h"
#include "ServoTask.h"
#include "TerminalTask.h"
#include "ServoTask.h"
#include "TapeTach.h"
#include "MotorDAC.h"
#include "IOExpander.h"
#include "Diag.h"

Semaphore_Handle g_semaSPI;
Semaphore_Handle g_semaServo;
Semaphore_Handle g_semaTransportMode;

extern IOExpander_Handle g_handleSPI1;
extern IOExpander_Handle g_handleSPI2;

#if BUTTON_INTERRUPTS != 0
Event_Handle g_eventSPI;
#endif

/* Static Function Prototypes */
Int main();
Void MainControlTask(UArg a0, UArg a1);
bool ReadSerialNumber(I2C_Handle handle, uint8_t ui8SerialNumber[16]);

//*****************************************************************************
// Main Program Entry Point
//*****************************************************************************

Int main()
{
    Error_Block eb;
    Mailbox_Params mboxParams;
    Task_Params taskParams;

    /* Enables Floating Point Hardware Unit */
    FPUEnable();
    /* Allows the FPU to be used inside interrupt service routines */
    FPULazyStackingEnable();

    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initUART();
    Board_initI2C();
    Board_initSPI();
    Board_initADC();

    /*
     * Create a mailbox message queues for the transport control/command tasks
     */

    /* Create transport controller task mailbox */

    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxCommander = Mailbox_create(sizeof(uint8_t), 8, &mboxParams, &eb);

    /* Create servo controller task mailbox */

    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxController = Mailbox_create(sizeof(CMDMSG), 8, &mboxParams, &eb);

    /* Create a global binary semaphores for serialized access items */

    Error_init(&eb);
    g_semaSPI = Semaphore_create(1, NULL, &eb);

    Error_init(&eb);
    g_semaServo = Semaphore_create(1, NULL, &eb);

    Error_init(&eb);
    g_semaTransportMode = Semaphore_create(1, NULL, &eb);

    /*
     * Now start the main application button polling task
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 13;

    if (Task_create(MainControlTask, &taskParams, &eb) == NULL)
        System_abort("MainControlTask!\n");

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
    i2cParams.transferMode  = I2C_MODE_BLOCKING;
    i2cParams.bitRate       = I2C_100kHz;
    g_handleI2C0 = I2C_open(Board_I2C0, &i2cParams);

    /* Open I2C1 for U9 AT24CS01 EPROM/Serial# */
    I2C_Params_init(&i2cParams);
    i2cParams.transferMode  = I2C_MODE_BLOCKING;
    i2cParams.bitRate       = I2C_100kHz;
    g_handleI2C1 = I2C_open(Board_I2C1, &i2cParams);

    /*
     * Open the SPI ports for peripherals we need to communicate with.
     */

    /* Initialize the SPI-0 to the reel motor DAC's */
    MotorDAC_initialize();

    /* Initialize the SPI-1 & SPI-2 to the I/O expanders */
    IOExpander_initialize();

    /* Read the serial number into memory */
    ReadSerialNumber(g_handleI2C1, g_ui8SerialNumber);
}

//*****************************************************************************
// The main application initialization, setup and button controler task.
//*****************************************************************************

#if BUTTON_INTERRUPTS != 0

/**************** INTERRUPT DRIVEN BUTTON EVENTS ******************/

Void MainControlTask(UArg a0, UArg a1)
{
	int status;
    uint8_t blink = 0;
    uint8_t intcap = 0;
    uint8_t intflag;
    uint8_t intmask;
    uint8_t mask;
    uint8_t tout_prev;
    uint8_t mode_prev;
	uint32_t ticks;
	uint32_t tdiff;
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
    g_dip_switch      = (mode_prev & M_DIPSW_MASK);

    /* Read the initial transport switch states */
    GetTransportSwitches(&tout_prev);
    g_tape_out_flag   = (tout_prev & S_TAPEOUT) ? 1 : 0;

    /* Read the system config parameters from storage */
    status = SysParamsRead(&g_sys);

    /* Detect tape width from head stack mounted */
    g_tape_width = (GPIO_read(Board_TAPE_WIDTH) == 0) ? 2 : 1;

    /* Initialize servo loop controller data */
    g_servo.mode              = MODE_HALT;
    g_servo.offset_sample_cnt = 0;
    g_servo.offset_null_sum   = 0;
    g_servo.tsense_sum        = 0;
    g_servo.tsense_sample_cnt = 0;
    g_servo.dac_halt_takeup   = 0;
    g_servo.dac_halt_supply   = 0;
	g_servo.play_tension_gain = g_sys.play_tension_gain;
	g_servo.play_boost_count  = 0;
	g_servo.rpm_takeup        = 0;
	g_servo.rpm_takeup_sum    = 0;
	g_servo.rpm_supply        = 0;
	g_servo.rpm_supply_sum    = 0;
	g_servo.rpm_sum_cnt       = 0;

    /* Servo's start in halt mode! */
    SET_SERVO_MODE(MODE_HALT);

    /* Create the transport command/control and terminal tasks.
     * The three tasks provide the bulk of the system control services.
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 9;
    if (Task_create(TransportCommandTask, &taskParams, &eb) == NULL)
        System_abort("TransportCommandTask()!n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 10;
    if (Task_create(TransportControllerTask, &taskParams, &eb) == NULL)
        System_abort("TransportControllerTask()!n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1664;
    taskParams.priority  = 1;
    if (Task_create(TerminalTask, &taskParams, &eb) == NULL)
        System_abort("TerminalTask()!\n");

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
    taskParams.stackSize = 800;
    taskParams.priority  = 15;

    if (Task_create(ServoLoopTask, &taskParams, &eb) == NULL)
        System_abort("ServoLoopTask()!\n");

    /****************************************************************
     * Enter the main application button processing loop forever.
     ****************************************************************/

    /* Send initial tape arm out state to transport ctrl/cmd task */

    mask = (g_tape_out_flag) ? S_TAPEOUT : S_TAPEIN;
    Mailbox_post(g_mailboxCommander, &mask, 10);

    /* Blink all lamps if error reading EPROM config data, otherwise
     * blink each lamp sequentially on success.
     */

    if (status != 0)
        LampBlinkError();
    else
        LampBlinkChase();

    /* Set initial status blink LED mask */
    g_lamp_blink_mask = L_STAT1;

    /****************************************************************
     * Enter the main application button processing loop forever.
     ****************************************************************/

    for(;;)
    {
    	/* Wait for ANY I/O expander ISR events to be posted */
    	UInt events = Event_pend(g_eventSPI, Event_Id_NONE, Event_Id_00 | Event_Id_01, 50);

    	/* U5 (SSI1) : SPI MCP23S17SO TRANSPORT SWITCHES & LAMPS */

    	if (events & Event_Id_00)
    	{
    		/* Read the interrupt flag register (bit that caused interrupt)
    		 * and the interrupt capture data register.
    		 */

    		/* Acquire the semaphore for exclusive access */
    	    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    	    {
    	    	/* Read the interrupt flag register (bit that caused interrupt) */
    	    	MCP23S17_read(g_handleSPI1, MCP_INTFA, &intflag);

    	    	/* Read the interrupt capture data register */
    	    	MCP23S17_read(g_handleSPI1, MCP_INTCAPA, &intcap);

    	    	Semaphore_post(g_semaSPI);
    	    }

    		/* Clear the GPIO interrupt status from U5 */
    		GPIO_clearInt(Board_INT1A);

            /* Determine if the tape out arm is active or not
             * and send any change in the switch state to
             * the transport control/command task.
             */

			if (intcap & ~(S_REC))
			{
				/* Rising edge interrupt, button pressed */
				ticks = Clock_getTicks();

				/* Save bit mask that generated the interrupt */
				intmask = intflag;
			}
			else
			{
				/* Falling edge interrupt, button released */
				tdiff = Clock_getTicks() - ticks;

				if (tdiff > g_sys.debounce)
				{
					/* First process the tape arm out switch */
					if (intmask & S_STOP)
					{
						mask = S_STOP | (intcap & S_REC);;
						Mailbox_post(g_mailboxCommander, &mask, 10);
					}
					else if (intmask & S_PLAY)
					{
						mask = S_PLAY | (intcap & S_REC);
						Mailbox_post(g_mailboxCommander, &mask, 10);
					}
					else if (intmask & S_FWD)
					{
						mask = S_FWD;
						Mailbox_post(g_mailboxCommander, &mask, 10);
					}
					else if (intmask & S_REW)
					{
						mask = S_REW;
						Mailbox_post(g_mailboxCommander, &mask, 10);
					}
					else if (intmask & S_LDEF)
					{
						mask = S_LDEF;
						Mailbox_post(g_mailboxCommander, &mask, 10);
					}
				}
    		}
    	}

        /* If no interrupt events, handle tape out arm state & LED's */

    	if (!events)
    	{
    		/* Poll the tape out arm switch state */

    		GetTransportSwitches(&mask);

    		mask &= S_TAPEOUT;

    		if (mask != tout_prev)
        	{
	            /* Save the new state */
	            tout_prev = mask;

	            /* Set the tape out arm state */
	            g_tape_out_flag = (mask & S_TAPEOUT) ? 1 : 0;

	            mask = (g_tape_out_flag) ? S_TAPEOUT : S_TAPEIN;

	            /* Send the button press to transport ctrl/cmd task */
	            Mailbox_post(g_mailboxCommander, &mask, 10);
        	}

    		/* Poll the tape speed select and DIP switch states */

            GetModeSwitches(&mask);

            if (mask != mode_prev)
            {
            	/* We debounced a switch change, reset and process it */
                mode_prev = mask;

                /* Save the transport speed select setting */
                g_high_speed_flag = (mask & M_HISPEED) ? 1 : 0;

                /* Save the updated DIP switch settings */
                g_dip_switch = mask & M_DIPSW_MASK;
            }

			/* See if its time to blink any LED(s) mask */

			if (++blink >= 10)
			{
				blink = 0;
				g_lamp_mask ^= g_lamp_blink_mask;
			}

			/* Set any new led/lamp state. We only update the LED
			 * output port if the lamp mask state changed.
			 */

			if (g_lamp_mask != g_lamp_mask_prev)
			{
				/* Set the new lamp state */
				SetLamp(g_lamp_mask);

				/* Update the previous lamp state*/
				g_lamp_mask_prev = g_lamp_mask;
			}
    	}
    }
}

#else

/**************** POLLED TRANSPORT BUTTON EVENTS ******************/

#define DEBOUNCE	6

Void MainControlTask(UArg a0, UArg a1)
{
    uint8_t count = 0;
    uint8_t bits = 0x00;
    uint8_t temp;
    uint8_t tran_prev = 0xff;
    uint8_t mode_prev = 0xff;
    uint8_t tout_prev = 0xff;
    uint8_t debounce_tran = 0;
    uint8_t debounce_mode = 0;
    uint8_t debounce_tout = 0;
    int status;
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
    g_dip_switch      = (mode_prev & M_DIPSW_MASK);

    /* Read the initial transport switch states */
    GetTransportSwitches(&tout_prev);
    g_tape_out_flag   = (tout_prev & S_TAPEOUT) ? 1 : 0;

    /* Read the system config parameters from storage */
    status = SysParamsRead(&g_sys);

    /* Detect tape width from head stack mounted */
    g_tape_width = (GPIO_read(Board_TAPE_WIDTH) == 0) ? 2 : 1;

    /* Initialize servo loop controller data */
    g_servo.mode              = MODE_HALT;
    g_servo.offset_sample_cnt = 0;
    g_servo.offset_null_sum   = 0;
    g_servo.tsense_sum        = 0;
    g_servo.tsense_sample_cnt = 0;
    g_servo.dac_halt_takeup   = 0;
    g_servo.dac_halt_supply   = 0;
	g_servo.play_tension_gain = g_sys.play_tension_gain;
	g_servo.play_boost_count  = 0;
	g_servo.rpm_takeup        = 0;
	g_servo.rpm_takeup_sum    = 0;
	g_servo.rpm_supply        = 0;
	g_servo.rpm_supply_sum    = 0;
	g_servo.rpm_sum_cnt       = 0;

    /* Servo's start in halt mode! */
    SET_SERVO_MODE(MODE_HALT);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 9;
    if (Task_create(TransportCommandTask, &taskParams, &eb) == NULL)
        System_abort("TransportCommandTask()!n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 10;
    if (Task_create(TransportControllerTask, &taskParams, &eb) == NULL)
        System_abort("TransportControllerTask()!n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 2048;
    taskParams.priority  = 1;
    if (Task_create(TerminalTask, &taskParams, &eb) == NULL)
        System_abort("TerminalTask()!\n");

    /* Initialize the tape tach timers and interrupt.
     * This tach provides the tape speed from the tape
     * counter roller.
     */

    TapeTach_initialize();

    /* Start the transport servo loop timer clock running.
     * We create an auto-reload periodic timer for the servo loop.
     * Note we are servicing the servo task at 500 Hz.
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 15;

    if (Task_create(ServoLoopTask, &taskParams, &eb) == NULL)
        System_abort("Servo loop task create failed!\n");

    /****************************************************************
     * Enter the main application button processing loop forever.
     ****************************************************************/

    /* Send initial tape arm out state to transport ctrl/cmd task */

    temp = (g_tape_out_flag) ? S_TAPEOUT : S_TAPEIN;
    Mailbox_post(g_mailboxCommander, &temp, 10);

    /* Blink all lamps if error reading EPROM config data, otherwise
     * blink each lamp sequentially on success.
     */

    if (status != 0)
        LampBlinkError();
    else
        LampBlinkChase();

    /* Set initial status blink LED mask */
    g_lamp_blink_mask = L_STAT1;

    for(;;)
    {
        /* Blink heartbeat LED1 on the transport interface card */
        if (++count >= 125)
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
            if (++debounce_tout >= DEBOUNCE)
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

        temp = bits & ~(S_REC);

        if (temp != tran_prev)
        {
            if (++debounce_tran >= DEBOUNCE)
            {
                debounce_tran = 0;

                /* Debounced a button press, send it to transport task */
                tran_prev = temp;

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
            if (++debounce_mode >= DEBOUNCE)
            {
                debounce_mode = 0;

                /* We debounced a switch change, reset and process it */
                mode_prev = bits;

                /* Save the transport speed select setting */
                g_high_speed_flag = (bits & M_HISPEED) ? 1 : 0;

                /* Save the updated DIP switch settings */
                g_dip_switch = bits & M_DIPSW_MASK;
            }
        }

        /* delay for 10ms and loop */
        Task_sleep(5);
    }
}

#endif

//*****************************************************************************
// Set default runtime values
//*****************************************************************************

void InitSysDefaults(SYSPARMS* p)
{
    /* default servo parameters */
    p->version                  = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    p->debug                    = 0;        /* debug mode 0=off                 */

    p->sysflags					= SF_BRAKES_STOP_PLAY | SF_ENGAGE_PINCH_ROLLER;

    p->vel_detect_threshold     = 5;        /* 10 pulses or less = no velocity  */
    p->null_offset_gain         = 3;        /* null offset gain */
    p->tension_sensor_gain      = 2;

    p->lifter_settle_time       = 600;      /* tape lifter settling delay in ms */
    p->brake_settle_time        = 450;
    p->play_settle_time			= 800;		/* play after shuttle settle time   */
    p->pinch_settle_time        = 250;      /* start 250ms after pinch roller   */
    p->record_pulse_time     	= REC_PULSE_TIME;
    p->rechold_settle_time    	= REC_SETTLE_TIME;
    p->debounce                 = 30;

    p->stop_supply_tension      = 250;      /* supply tension level (0-DAC_MAX) */
    p->stop_takeup_tension      = 250;      /* takeup tension level (0-DAC_MAX) */
    p->stop_max_torque          = DAC_MAX;  /* max stop servo torque (0-DAC_MAX)*/
    p->stop_min_torque          = 10;       /* min stop servo torque            */
    p->stop_brake_torque        = 650;    	/* max dynamic stop brake torque   */

    p->shuttle_supply_tension   = 250;      /* shuttle supply reel tension      */
    p->shuttle_takeup_tension   = 250;      /* shuttle takeup reel tension      */
    p->shuttle_max_torque       = DAC_MAX;  /* shuttle max torque               */
    p->shuttle_min_torque       = 10;       /* shuttle min torque               */
    p->shuttle_velocity         = 450;      /* max shuttle velocity             */
    p->shuttle_slow_offset      = 60;       /* offset to reduce velocity at     */
    p->shuttle_slow_velocity    = 0;        /* reduce velocity to speed         */
    p->shuttle_servo_pgain      = 75;       /* shuttle mode servo P-gain        */
    p->shuttle_servo_igain      = 16;       /* shuttle mode servo I-gain        */
    p->shuttle_servo_dgain      = 3;        /* shuttle mode servo D-gain        */

    p->play_tension_gain        = 9;        /* play tension velocity gain factor*/
    p->play_lo_supply_tension   = 186;      /* supply tension level (0-DAC_MAX) */
    p->play_lo_takeup_tension   = 186;      /* takeup tension level (0-DAC_MAX) */
    p->play_hi_supply_tension   = 188;      /* supply tension level (0-DAC_MAX) */
    p->play_hi_takeup_tension   = 188;      /* takeup tension level (0-DAC_MAX) */
    p->play_max_torque          = DAC_MAX;  /* play mode max torque (0-DAC_MAX) */
    p->play_min_torque          = 10;       /* play mode min torque (0-DAC_MAX) */
    p->play_lo_boost_time       = 1500;     /* play mode accel boost from stop  */
    p->play_lo_boost_step       = 10;
    p->play_hi_boost_time       = 3000;     /* play mode accel boost from stop  */
    p->play_hi_boost_step       = 8;
    p->play_lo_boost_start      = DAC_MAX;
    p->play_lo_boost_end        = 105;
    p->play_hi_boost_start      = DAC_MAX;
    p->play_hi_boost_end        = 235;		// 215;

    p->reserved3                = 0;        /* reserved */
    p->reserved4                = 0;        /* reserved */
}

//*****************************************************************************
// Write system parameters from our global settings buffer to EEPROM.
//
// Returns:  0 = Sucess
//          -1 = Error writing EEPROM data
//*****************************************************************************

int32_t SysParamsWrite(SYSPARMS* sp)
{
    int32_t rc = 0;

    sp->version = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    sp->magic   = MAGIC;

    rc = EEPROMProgram((uint32_t *)sp, 0, sizeof(SYSPARMS));

    System_printf("Writing System Parameters %d\n", rc);
    System_flush();

    return rc;
 }

//*****************************************************************************
// Read system parameters into our global settings buffer from EEPROM.
//
// Returns:  0 = Success
//          -1 = Error reading flash
//
//*****************************************************************************

int32_t SysParamsRead(SYSPARMS* sp)
{
    InitSysDefaults(sp);

    EEPROMRead((uint32_t *)sp, 0, sizeof(SYSPARMS));

    if (sp->magic != MAGIC)
    {
        System_printf("ERROR Reading System Parameters - Using Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    if (sp->version != MAKEREV(FIRMWARE_VER, FIRMWARE_REV))
    {
        System_printf("WARNING New Firmware Version - Using Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    return 0;
}

//*****************************************************************************
// This function attempts to read the unique serial number from
// the AT24CS01 I2C serial EPROM and serial# number device.
//*****************************************************************************

bool ReadSerialNumber(I2C_Handle handle, uint8_t ui8SerialNumber[16])
{
	bool			ret;
	uint8_t			txByte;
	I2C_Transaction i2cTransaction;

    /* default invalid serial number is all FF's */
    memset(ui8SerialNumber, 0xFF, sizeof(ui8SerialNumber));

	/* Note the Upper bit of the word address must be set
	 * in order to read the serial number. Thus 80H would
	 * set the starting address to zero prior to reading
	 * this sixteen bytes of serial number data.
	 */

	txByte = 0x80;

	i2cTransaction.slaveAddress = Board_AT24CS01_SERIAL_ADDR;
	i2cTransaction.writeBuf     = &txByte;
	i2cTransaction.writeCount   = 1;
	i2cTransaction.readBuf      = ui8SerialNumber;
	i2cTransaction.readCount    = 16;

	ret = I2C_transfer(handle, &i2cTransaction);

	if (!ret)
	{
		System_printf("Unsuccessful I2C transfer\n");
		System_flush();
	}

	return ret;
}
