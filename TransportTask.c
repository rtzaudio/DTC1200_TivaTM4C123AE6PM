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
#include "TransportTask.h"
#include "IOExpander.h"
#include "TapeTach.h"

/* Static Function Prototypes */
static void ResetServoPlay(void);
static void ResetServoPID(void);
static void HandleImmediateCommand(CMDMSG *p);

extern Semaphore_Handle g_semaServo;

//*****************************************************************************
// Reset PLAY servo parameters. This gets called every time prior to the
// transport controller entering play mode. Here we reset all the play boost
// ramp up counters that control play boost logic before the play servo mode
// begins in the servo task.
//*****************************************************************************

void ResetServoPlay(void)
{
    Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

    /* Reset play mode capture data buffer to zeros */
#if (CAPDATA_SIZE > 0)
    size_t i;
    for (i=0; i < CAPDATA_SIZE; i++)
    {
        g_capdata[i].dac_takeup = 0.0f;
        g_capdata[i].dac_supply = 0.0f;
        g_capdata[i].vel_takeup = 0.0f;
        g_capdata[i].vel_supply = 0.0f;
        g_capdata[i].rad_supply = 0.0f;
        g_capdata[i].rad_takeup = 0.0f;
        g_capdata[i].tape_tach  = 0.0f;
        g_capdata[i].tension    = 0.0f;
    }
    g_capdata_count = 0;
#endif

    g_servo.play_boost_count = 500;
    g_servo.play_boost_index = 0;

    /* Initialize the play servo data items */
    if (g_high_speed_flag)
    {
        /* Reset the tension values */
        g_servo.play_supply_tension    = (float)g_sys.play_hi_supply_tension;
        g_servo.play_takeup_tension    = (float)g_sys.play_hi_takeup_tension;
        g_servo.play_boost_supply_gain = g_sys.play_hi_boost_supply_gain;
        g_servo.play_boost_takeup_gain = g_sys.play_hi_boost_takeup_gain;
        g_servo.play_boost_end         = g_sys.play_hi_boost_end;
    }
    else
    {
        /* Reset the tension values */
        g_servo.play_supply_tension    = (float)g_sys.play_lo_supply_tension;
        g_servo.play_takeup_tension    = (float)g_sys.play_lo_takeup_tension;
        g_servo.play_boost_supply_gain = g_sys.play_lo_boost_supply_gain;
        g_servo.play_boost_takeup_gain = g_sys.play_lo_boost_takeup_gain;
        g_servo.play_boost_end         = g_sys.play_lo_boost_end;
    }

    TapeTach_reset();

    Semaphore_post(g_semaServo);
}

//*****************************************************************************
// Reset shuttle PID servo parameters. This gets called every time prior
// to the transport controller entering shuttle mode.
//*****************************************************************************

void ResetServoPID(void)
{
    Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

    fpid_init(&g_servo.fpid,
             g_sys.shuttle_servo_pgain,     // P-gain
             g_sys.shuttle_servo_igain,     // I-gain
             g_sys.shuttle_servo_dgain,     // D-gain
             DAC_MAX_F,
             PID_TOLERANCE_F);              // PID deadband

    Semaphore_post(g_semaServo);
}

//*****************************************************************************
// Set the next or immediate transport mode requested.
//*****************************************************************************

void QueueTransportCommand(uint8_t command, uint8_t opcode)
{
    CMDMSG msg;

    msg.command = command;      /* Set the command message type */
    msg.opcode  = opcode;       /* Set any cmd specfic op-code  */

    Mailbox_post(g_mailboxController, &msg, 10);
}

//*****************************************************************************
// Enable record mode! This function is called to enable record mode on the
// transport for any channels with record mode armed. First we must set the
// record hole line high to hold any armed channels in record when strobed.
// Then we generate a short record latch pulse (~20ms) to latch all of the
// armed record relays on each channel. This pulse activates record on any
// channels and remains actvie until the record hold line is pulled low again.
//*****************************************************************************

void RecordEnable(void)
{
    if (!(GetTransportMask() & T_RECH))
    {
        /* 1) Enable the record hold line */
        SetTransportMask(T_RECH, 0);

        /* 2) Settling time for record hold */
        Task_sleep(g_sys.rechold_settle_time);

        /* 3) Generate a record latch pulse */
        SetTransportMask(T_RECP, 0);

        /* 4) Sleep for pulse length duration */
        Task_sleep(g_sys.record_pulse_time);

        /* 5) Drop the record pulse line low */
        SetTransportMask(0, T_RECP);

        /* 6) Turn on the record button lamp */
        g_lamp_mask |= L_REC;
    }
}

//*****************************************************************************
// Disable record mode if active. This function is called by the transport
// when changing modes to disable record if active. It gets called when the
// user hits any button that changes mode (Fwd, Rew, Stop, etc) to release
// the record hold latch on any channels that have record mode active.
//*****************************************************************************

void RecordDisable(void)
{
    /* Is record mode currently active? */
    if (GetTransportMask() & T_RECH)
    {
        /* Yes, disable the record hold latch */
        SetTransportMask(0, T_RECH);

        /* Turn of the rec indicator LED and lamps */
        g_lamp_mask &= ~(L_REC);
    }
}

/*****************************************************************************
 * MAIN TRANSPORT COMMAND TASK
 *
 * This is the second highest priority system task that handles all
 * of the transport control logic for tape movement. This task reads
 * the message queue for transport button control events and processes
 * accordingly depending on the transport state and mode.
 *****************************************************************************/

Void TransportCommandTask(UArg a0, UArg a1)
{
    uint8_t mbutton;
    uint8_t firststate = 1;

    /* Enter the main transport controller task loop. Here we process button presses
     * from the message queue and handle various transport mode control states.
     */
    for( ;; )
    {
	   	/* Wait for a button press or switch change event */
    	if (Mailbox_pend(g_mailboxCommander, &mbutton, 100) == TRUE)
		{
			/* Check for tape out (EOT) switch change? */
		    if (mbutton & S_TAPEOUT)
		    {
		        /* If transport was NOT in HALT mode, set HALT mode */
		        if (!IS_SERVO_MODE(MODE_HALT) || firststate)
		        {
		        	firststate = 0;
		        	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_HALT);
		        	continue;
		        }
		    }
		    else if (mbutton & S_TAPEIN)
		    {
		        /* If transport was in HALT mode (out of tape), set STOP mode */
		        if (IS_SERVO_MODE(MODE_HALT) || firststate)
		        {
		        	firststate = 0;
		            QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_STOP);
		            continue;
		        }
		    }
		    else if (mbutton == S_LDEF)
            {
		        /* lift defeat button */
                QueueTransportCommand(CMD_TOGGLE_LIFTER, 0);
                continue;
            }

		    /* Ignore transport control buttons in halt mode */

		    if (IS_SERVO_MODE(MODE_HALT))
		        continue;

		    /* Mask out the tape out indicator bits */
		    mbutton &= ~(S_TAPEOUT | S_TAPEIN);

		    if (!mbutton)
		    	continue;

		    /* Stop only button pressed? */
		    if (mbutton == S_STOP)
		    {
		        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_STOP);
		    }
            else if (mbutton == S_FWD)              /* fast fwd button */
            {
                QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_FWD);
            }
            else if (mbutton == (S_FWD|S_REC))		/* fast fwd + rec button */
            {
                QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_FWD|M_LIBWIND);
            }
            else if (mbutton == S_REW)              /* rewind button */
            {
                QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_REW);
            }
            else if (mbutton == (S_REW|S_REC))     /* rewind + rec button */
            {
                QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_REW|M_LIBWIND);
            }
            else if (mbutton == S_PLAY)             /* play only button pressed? */
            {
                QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_PLAY);
            }
		    else if (mbutton == (S_STOP | S_REC))   /* stop & record button? */
		    {
		    	if (IS_SERVO_MODE(MODE_PLAY))       /* punch out */
		    		QueueTransportCommand(CMD_STROBE_RECORD, 0);
		    }
		    else if ((mbutton & (S_PLAY | S_REC)) == (S_PLAY | S_REC))
		    {
	            /* play+rec buttons pressed? */
		        /* Already in play mode? */
		        if (IS_SERVO_MODE(MODE_PLAY))
		        {
		        	/* Is transport already in record mode? */
		        	if (GetTransportMask() & T_RECH)
		        		QueueTransportCommand(CMD_STROBE_RECORD, 0);	/* punch out */
		        	else
		        		QueueTransportCommand(CMD_STROBE_RECORD, 1);	/* punch in */
		        }
		        else if (IS_SERVO_MODE(MODE_STOP))
		        {
		        	/* Startup PLAY in REC mode */
		            QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_PLAY|M_RECORD);
		        }
		    }
	    }
    }
}

//*****************************************************************************
// Transport servo controller task. This task controls and directs the
// main reel servo loop task. It handles any required transport control
// logic when switching between the various transport servo modes (eg, rewind
// to stop, fwd to play, etc). Transport control commands are processed on the
// fly asynchronously. All commands pend until completion of the current
// command and transport states required. If a new command is received prior
// to completing a pending command, the new mode will replace the pending
// command after taking the appropriate steps required, if any, to transition
// to the new mode requested. This task is the second highest priority in
// the system.
//*****************************************************************************

Void TransportControllerTask(UArg a0, UArg a1)
{
    uint8_t mask;
    uint8_t mode;
    uint8_t record;
    uint8_t lamp_mask;
    uint8_t last_mode_completed = 0xFF;
    uint8_t last_mode_requested = 0xFF;
    uint8_t prev_mode_requested = 0xFF;
    uint8_t mode_pending = 0;
    uint32_t stoptimer = 0;
    CMDMSG msg;

    for(;;)
    {
        /* Wait for a mode change command from the transport controller queue */

        if (Mailbox_pend(g_mailboxController, &msg, 25) == TRUE)
        {

            /* Process immediate command messages first */

            if (msg.command != CMD_TRANSPORT_MODE)
            {
                /* Dispatch any immediate commands */
                HandleImmediateCommand(&msg);
                /* Loop back waiting for the next message */
                continue;
            }

            /* Otherwise, we received a command to change the transport mode */

            /* mask out only the mode bits */
            mode = msg.opcode & MODE_MASK;

            /* Skip if same command requested */
            if ((last_mode_completed == mode) && (mode_pending == 0))
                continue;

            prev_mode_requested = last_mode_requested;

            last_mode_requested = mode;

            //System_printf("%u\n", mode);
            //System_flush();

            /* Reset pending record enable and pending stop timer counter */
            record = stoptimer = 0;

            /* Set or Reset error LED indicators. The STAT2 LED
             * indicates tape out. STAT3 LED indicates timeout
             * error while waiting for pending motion stop.
             */
            if (mode == MODE_HALT)
                g_lamp_mask |= L_STAT2;
            else
                g_lamp_mask &= ~(L_STAT2 | L_STAT3);

            /* Process the requested mode change command */

            switch(mode)
            {
                case MODE_HALT:

                    /* Disable record if active! */
                    RecordDisable();

                    /* All lamps off, diag leds preserved */
                    g_lamp_mask &= L_LED_MASK;

                    /* TAPE OUT - Stop capstan servo, engage brakes,
                     * disengage lifters, disengage pinch roller,
                     * disable record.
                     */
                    SetTransportMask(T_BRAKE, 0xFF);

                    /* Set servo mode to HALT */
                    SET_SERVO_MODE(MODE_HALT);

                    last_mode_completed = MODE_HALT;
                    mode_pending = 0;
                    break;

                case MODE_STOP:

                    /* Disable record if active! */
                    RecordDisable();

                    //if (mode_pending)
                    //  break;

                    /* Ignore if already in stop mode */
                    //if (IS_SERVO_MODE(MODE_STOP))
                    //  break;

                    /* Set the lamps to indicate the stop mode */
                    if (last_mode_completed == MODE_FWD)
                        lamp_mask = L_FWD;
                    else if (last_mode_completed == MODE_REW)
                        lamp_mask = L_REW;
                    //else if (last_mode_completed == MODE_PLAY)
                    //  lamp_mask = L_PLAY;
                    else
                        lamp_mask = 0;

                    /* If we're blinking the stop lamp during pending stop
                     * requests, then turn on the STOP lamp initially also.
                     */
                    if (!(g_dip_switch & M_DIPSW2))
                        lamp_mask |= L_STOP;

                    /* Stop and new lamp mask, diag leds preserved */
                    g_lamp_mask = (g_lamp_mask & L_LED_MASK) | lamp_mask;

                    /* Disable record if active */
                    SetTransportMask(0, T_PROL | T_SERVO | T_RECH);

                    /* Set the reel servos for stop mode */
                    SET_SERVO_MODE(MODE_STOP);

                    mode_pending = MODE_STOP;
                    break;

                case MODE_PLAY:

                    /* Ignore if already in play mode */
                    if (IS_SERVO_MODE(MODE_PLAY))
                        break;

                    /* Don't engage play if another mode is pending! */
                    if (mode_pending)
                        break;

                    /* Turn on the play lamp */
                    //g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_PLAY;

                    /* save upper bit as it indicates record+play mode */
                    record = (msg.opcode & M_RECORD) ? 1 : 0;

                    /* Set the reel servos to stop mode initially */
                    SET_SERVO_MODE(MODE_STOP);

                    mode_pending = MODE_PLAY;
                    break;

                case MODE_REW:

                    /* Disable record if active! */
                    RecordDisable();

                    /* Ignore if already in rew mode */
                    if (IS_SERVO_MODE(MODE_REW))
                        break;

                    /* Light the rewind lamp only */
                    g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_REW;

                    /* REW - stop the capstan servo, disengage brakes,
                     * engage lifters, disengage pinch roller,
                     * disable record.
                     */
                    SetTransportMask(T_TLIFT, T_SERVO | T_PROL | T_RECH | T_BRAKE);

                    /* Initialize shuttle mode PID values */
                    ResetServoPID();

                    /* Set the servo velocity parameter */
                    g_servo.shuttle_velocity = (msg.opcode & M_LIBWIND) ? g_sys.shuttle_lib_velocity : g_sys.shuttle_velocity;

                    // 500 ms delay for tape lifter settling time
                    if (!IS_SERVO_MOTION())
                        Task_sleep(g_sys.lifter_settle_time);

                    /* Set servos to REW mode */
                    SET_SERVO_MODE(MODE_REW);

                    last_mode_completed = MODE_REW;
                    mode_pending = 0;
                    break;

                case MODE_FWD:

                    /* Disable record if active! */
                    RecordDisable();

                    /* Ignore if already in ffwd mode */
                    if (IS_SERVO_MODE(MODE_FWD))
                        break;

                    /* Reset all lamps & light fast fwd lamp */
                    g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_FWD;

                    /* FWD - stop the capstan servo, disengage brakes,
                     * engage lifters, disengage pinch roller,
                     * disable record.
                     */
                    SetTransportMask(T_TLIFT, T_SERVO | T_PROL | T_RECH | T_BRAKE);

                     /* Initialize the shuttle tension sensor and velocity PID data */
                    ResetServoPID();

                    /* Set the servo velocity parameter */
                    g_servo.shuttle_velocity = (msg.opcode & M_LIBWIND) ? g_sys.shuttle_lib_velocity : g_sys.shuttle_velocity;

                    // 500 ms delay for tape lifter settling time
                    if (!IS_SERVO_MOTION())
                        Task_sleep(g_sys.lifter_settle_time);

                    /* Set servos to FWD mode */
                    SET_SERVO_MODE(MODE_FWD);

                    last_mode_completed = MODE_FWD;
                    mode_pending = 0;
                    break;

                default:
                    /* Invalid Command??? */
                    g_lamp_mask |= L_STAT3;
                    break;
            }
        }
        else
        {
            /* The message queue has timed out while waiting for a command. We use this
             * as a timer mechanism and check to see if we're currently waiting for the
             * transport to stop all motion before completing any previous stop/play command.
             * If the timeout stop count exceeds the limit then we timed out waiting for motion
             * to stop and we assume the reels are still spinning (out of tape maybe?).
             * In this case we treat it as an error and just revert to stop mode.
             */

            if (!mode_pending)
                continue;

            /* 60 seconds motion stop detect timeout */

            if (++stoptimer >= 2400)
            {
                System_printf("Wait for STOP timeout!\n");
                System_flush();

                /* Stop lamp only, diag leds preserved */
                g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_STOP | L_STAT3;

                /* Set reel servos to stop */
                SET_SERVO_MODE(MODE_STOP);

                /* error, the motion didn't stop within timeout period */
                mode_pending = record = stoptimer = 0;

                last_mode_completed = MODE_STOP;
                continue;
            }

            /* Blink the stop lamp to indicate stop is pending */

            if (!(g_dip_switch & M_DIPSW2))
            {
                if ((stoptimer % 12) == 0)
                {
                    //if (mode_pending == MODE_PLAY)
                    //    g_lamp_mask ^= L_PLAY;
                    //else
                    //    g_lamp_mask ^= L_STOP;

                    if (last_mode_completed == MODE_REW)
                    	g_lamp_mask ^= L_REW;
                    else if (last_mode_completed == MODE_FWD)
                    	g_lamp_mask ^= L_FWD;
                    else
                        g_lamp_mask ^= L_STOP;
                }
            }

            /*** PROCESS PENDING COMMAND STATES ***/

            switch(mode_pending)
            {
                case MODE_STOP:

                    /* Has all motion stopped yet? */
                    if (last_mode_completed != MODE_PLAY)
                    {
                        if (IS_SERVO_MOTION())
                            break;
                    }

                    /* At this point, all motion has stopped from the last command
                     * that required a pending motion stop state. Here we process
                     * the final state portion of the previous pending command.
                     */

                    /* STOP capstan servo, disengage pinch roller & disable record. */
                    SetTransportMask(0, T_SERVO | T_PROL | T_RECH);

                    if ((prev_mode_requested == MODE_FWD) ||
                        (prev_mode_requested == MODE_REW) ||
                        (prev_mode_requested == MODE_PLAY))
                    {
                        if ((prev_mode_requested == MODE_PLAY) && (g_sys.sysflags & SF_BRAKES_STOP_PLAY))
                        {
                            /* STOP - Engage brakes, stop capstan servo,
                             *        disengage pinch roller, disable record.
                             */

                            /* Pre-brake delay, let servo stop loop have some effect */
                            Task_sleep(225);

                            /* Now apply the hard brakes */
                            SetTransportMask(T_BRAKE, T_SERVO | T_PROL | T_RECH);
                        }
                        else
                        {
                            /* STOP - Release brakes, stop capstan servo,
                             *        disengage pinch roller, disable record.
                             */
                            SetTransportMask(0, T_BRAKE | T_SERVO | T_PROL | T_RECH);
                        }

                        /* Brake settle time after stop (~300 ms) */
                        Task_sleep(g_sys.brake_settle_time);
                    }

                    /* Stop lamp only, diag LED's preserved */
                    g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_STOP;

                    /* Stop capstan servo, release brakes, release lifter,
                     * release pinch roller and end any record mode.
                     */

                    mask = GetTransportMask();

                    /* Leave lifter engaged at stop if enabled. */
                    if (g_sys.sysflags & SF_LIFTER_AT_STOP)
                    {
                        /* Assure lifter engaged */
                        SetTransportMask(T_TLIFT, T_SERVO | T_PROL | T_RECH);
                    }
                    else
                    {
                        /* Release the lifter */
                        SetTransportMask(0, T_SERVO | T_TLIFT | T_PROL | T_RECH);

                        /* Tape lifter settling Time */
                        if (mask & T_TLIFT)
                            Task_sleep(g_sys.lifter_settle_time);
                    }

                    /* Leave brakes engaged if brakes at stop enabled, otherwise release brakes */
                    if (g_sys.sysflags & SF_BRAKES_AT_STOP)
                        SetTransportMask(T_BRAKE, 0);
                    else
                        SetTransportMask(0, T_BRAKE);

                    last_mode_completed = MODE_STOP;
                    mode_pending = 0;
                    break;

                case MODE_PLAY:

                    /* Has all motion stopped yet? */
                    if (IS_SERVO_MOTION())
                        break;

                    /* All motion has stopped, allow 1 second settling
                     * time prior to engaging play after shuttle mode.
                     */

                    /* Play lamp only, diag leds preserved */
                    g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_PLAY;

                    if ((prev_mode_requested == MODE_FWD) || (prev_mode_requested == MODE_REW))
                    {
                        /* Setting time before engaging play after shuttle */
                        Task_sleep(g_sys.play_settle_time);
                    }

                    mask = GetTransportMask();

                    /* Disengage tape lifters & brakes. */
                    SetTransportMask(0, T_TLIFT | T_BRAKE);

                    /* Settling time for tape lifter release */
                    if (mask & T_TLIFT)
                    {
                        if (g_sys.sysflags & SF_LIFTER_AT_STOP)
                            Task_sleep(g_sys.lifter_settle_time);
                    }

                    /* Things happen pretty quickly from here. First we engage the
                     * pinch roller and allow it time to settle. Next we start
                     * the reel motors with PID values just set. Immediately
                     * afterward we must start the capstan motor so the tape
                     * can finally begin moving.
                     */

                    /* [1] Engage the pinch roller */
                    if (g_sys.sysflags & SF_ENGAGE_PINCH_ROLLER)
                    {
                        /* Engage the pinch roller */
                        SetTransportMask(T_PROL, 0);

                        /* Give pinch roller time to settle */
                        Task_sleep(g_sys.pinch_settle_time);
                    }

                    /* Set the play mode velocity */
                    ResetServoPlay();

                    /* [3] Now start the capstan servo motor */
                    SetTransportMask(T_SERVO, 0);

                    /* [2] Start the reel servos in PLAY mode */
                    SET_SERVO_MODE(MODE_PLAY);

                    /* [4] Enable record if record flag was set */
                    if (record)
                    {
                        record = 0;
                        RecordEnable();
                    }

                    last_mode_completed = MODE_PLAY;
                    mode_pending = 0;
                    break;

                default:
                    //System_printf("invalid pend stop state %d!\n", mode_pending);
                    //System_flush();
                	mode_pending = 0;
                    break;
            }
        }
    }
}

//*****************************************************************************
// This function handles immediate mode commands to set/toggle
// record mode or the tape lifters.
//*****************************************************************************

void HandleImmediateCommand(CMDMSG *p)
{
	int32_t mode = ServoGetMode();

    switch(p->command)
    {
        case CMD_STROBE_RECORD:
            /* Enabled, disable or toggle record mode! */
            if (mode == MODE_PLAY)
            {
                if (p->opcode == 0)
                {
                    /* punch out */
                    RecordDisable();
                }
                else if (p->opcode == 1)
                {
                    /* punch in */
                    RecordEnable();
                }
                else
                {
                    /* toggle record mode */
                    if (GetTransportMask() & T_RECH)
                        RecordDisable();
                    else
                        RecordEnable();
                }
            }
            break;

        case CMD_TOGGLE_LIFTER:
            /* Must be in HALT, STOP or PLAY mode */
            if ((mode == MODE_HALT) || (mode == MODE_STOP) || (mode == MODE_PLAY))
            {
                /* toggle lifter defeat */
                if (GetTransportMask() & T_TLIFT)
                    SetTransportMask(0, T_TLIFT);
                else
                    SetTransportMask(T_TLIFT, 0);
            }
            break;

        default:
            /* invalid command */
            break;
    }
}

// End-Of-File
