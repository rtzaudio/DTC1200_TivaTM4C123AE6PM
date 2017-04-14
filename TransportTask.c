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
#include "TachTimer.h"

/* Static Function Prototypes */
static void ResetPlayServo(void);
static void ResetStopServo(int state);

extern Semaphore_Handle g_semaServo;

//*****************************************************************************
// Reset PLAY servo parameters. This gets called every time prior to the
// transport controller entering play mode. Here we reset all the play boost
// ramp up counters that control play boost logic before the play servo mode
// begins in the servo task.
//*****************************************************************************

void ResetPlayServo(void)
{
    Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

	/* Reset play mode capture data buffer to zeros */
#if (CAPDATA_SIZE > 0)
	memset(g_capdata, 0, sizeof(g_capdata));
	g_capdata_count = 0;
#endif

	g_servo.play_tension_gain = g_sys.play_tension_gain;
	g_servo.play_boost_count  = 0;

	/* Initialize the play servo data items */
	if (g_high_speed_flag)
	{
		/* Reset the tension values */
		g_servo.play_supply_tension = g_sys.play_hi_supply_tension;
		g_servo.play_takeup_tension = g_sys.play_hi_takeup_tension;

		/* Reset the play boost counters */
		g_servo.play_boost_time     = g_sys.play_hi_boost_time;
		g_servo.play_boost_step     = g_sys.play_hi_boost_step;
		g_servo.play_boost_start    = g_sys.play_hi_boost_start;
		g_servo.play_boost_end      = g_sys.play_hi_boost_end;

		/* If play boost end config parameter is 0, then use
		 * the takeup tension parameter value instead.
		 */
		if (g_sys.play_hi_boost_end == 0)
			g_servo.play_boost_end = g_sys.play_hi_takeup_tension;
	}
	else
	{
		/* Reset the tension values */
		g_servo.play_supply_tension = g_sys.play_lo_supply_tension;
		g_servo.play_takeup_tension = g_sys.play_lo_takeup_tension;

		/* Reset the play boost counters */
		g_servo.play_boost_time     = g_sys.play_lo_boost_time;
		g_servo.play_boost_step     = g_sys.play_lo_boost_step;
		g_servo.play_boost_start    = g_sys.play_lo_boost_start;
		g_servo.play_boost_end      = g_sys.play_lo_boost_end;

		/* If play boost end config parameter is 0, then use
		 * the takeup tension parameter value instead.
		 */
		if (g_sys.play_lo_boost_end == 0)
			g_servo.play_boost_end = g_sys.play_lo_takeup_tension;
	}

	ResetTapeTach();

	Semaphore_post(g_semaServo);
}

//*****************************************************************************
// Reset STOP servo parameters. This function gets called every time the
// the machine enters stop servo mode. Here we reset the brake state machine
// and counters used by the stop servo task to perform dynamic braking.
//*****************************************************************************

void ResetStopServo(int state)
{
    //Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);
	//g_servo.stop_null_torque = 0;
	//g_servo.stop_brake_state  = 0;
	//Semaphore_post(g_semaServo);
}

//*****************************************************************************
// Set the next or immediate transport mode requested.
//*****************************************************************************

void QueueTransportCommand(uint8_t cmd, uint8_t op)
{
    CMDMSG msg;
    msg.cmd = cmd;		/* Set the command message type */
    msg.op  = op;		/* Set any cmd specfic op-code  */

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
		Task_sleep(5);

		/* 3) Generate a record latch pulse */
		SetTransportMask(T_RECP, 0);

		/* 4) Sleep for pulse length duration */
		Task_sleep(g_sys.record_pulse_length);

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
		    /* stop & record button pressed? */
		    else if (mbutton == (S_STOP | S_REC))
		    {
		    	if (IS_SERVO_MODE(MODE_PLAY))
		    		QueueTransportCommand(CMD_PUNCH, 0);	/* punch out */
		    }
		    else if (mbutton == S_PLAY)			    /* play only button pressed? */
		    {
	        	QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_PLAY);
		    }
		    /* play+rec buttons pressed? */
		    else if ((mbutton & (S_PLAY | S_REC)) == (S_PLAY | S_REC))
		    {
		        /* Already in play mode? */
		        if (IS_SERVO_MODE(MODE_PLAY))
		        {
		        	/* Is transport already in record mode? */
		        	if (GetTransportMask() & T_RECH)
		        		QueueTransportCommand(CMD_PUNCH, 0);	/* punch out */
		        	else
		        		QueueTransportCommand(CMD_PUNCH, 1);	/* punch in */
		        }
		        else if (IS_SERVO_MODE(MODE_STOP))
		        {
		        	/* Startup PLAY in REC mode */
		            QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_PLAY|M_RECORD);
		        }
		    }
		    else if (mbutton == S_FWD)      	/* fast fwd button */
		    {
		        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_FWD);
		    }
		    else if (mbutton == S_REW)      	/* rewind button */
		    {
		        QueueTransportCommand(CMD_TRANSPORT_MODE, MODE_REW);
		    }
		    else if (mbutton == S_LDEF)			/* lift defeat button */
		    {
		    	if (IS_SERVO_MODE(MODE_STOP) || IS_SERVO_MODE(MODE_PLAY))
			    	QueueTransportCommand(CMD_TOGGLE_LIFTER, 0);
		    }
		    else if (mbutton == S_REC)
		    {
#if 0
		        /* one-touch record if DIP switch #1 is on! */
		        if (g_sys.sysflags & SF_ONE_BUTTON_RECORD)
		        {
		            /* One-touch punch in/out mode */
		            if (IS_SERVO_MODE(MODE_PLAY))
		            {
		                /* Is transport already in record mode? */
		                if (GetTransportMask() & T_RECH)
		                	RecordDisable();
		                else
		                	RecordEnable();
		            }
		        }
#endif
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
	uint8_t temp;
	uint8_t playrec;
	uint8_t lamp_mask;
    long mode = UNDEFINED;
    long prev_mode = UNDEFINED;
    long last_mode = UNDEFINED;
    long pendstop = 0;
    long stoptimer = 0;
    long shuttling = 0;
    CMDMSG msg;

    for(;;)
    {
        /* Wait for a mode change command from the transport controller queue */

    	if (Mailbox_pend(g_mailboxController, &msg, 50) == TRUE)
	    {
			/* Process immediate command messages first */

			if (msg.cmd != CMD_TRANSPORT_MODE)
			{
				switch(msg.cmd)
				{
				case CMD_PUNCH:
					/* Punch in/out, must be in PLAY mode! */
    		    	if (IS_SERVO_MODE(MODE_PLAY))
    		    	{
    		    		if (msg.op)
    		    			RecordEnable();		/* punch in */
    		    		else
	    	        		RecordDisable();	/* punch out */
    		    	}
					break;

				case CMD_TOGGLE_LIFTER:
					/* Must be in STOP or PLAY mode */
			    	if (IS_SERVO_MODE(MODE_STOP) || IS_SERVO_MODE(MODE_PLAY))
			    	{
			    		/* toggle lifter defeat */
			    		if (GetTransportMask() & T_TLIFT)
			    			SetTransportMask(0, T_TLIFT);
			    		else
			    			SetTransportMask(T_TLIFT, 0);
			    	}
					break;
				}
				/* Loop back waiting for the next message */
				continue;
			}

			/* Otherwise, we received a command to change the current
			 * transport operating mode.
			 */

			temp = msg.op & MODE_MASK;

			/* Skip if same command requested */
    		if ((mode == UNDEFINED) && (temp == prev_mode))
        		continue;

		    /* Save the new mode currently requested */
            mode = temp;

            /* Get the previous mode executed */
            last_mode = prev_mode;

		    /* Reset any pending rec, stop cmd and timer counter */
            playrec = pendstop = stoptimer = 0;

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

			    	prev_mode = MODE_HALT;
					mode = UNDEFINED;
					break;

				case MODE_STOP:

					/* Disable record if active! */
	        		RecordDisable();

					/* Ignore if already in stop mode */
					if (IS_SERVO_MODE(MODE_STOP))
						continue;

					/* Set the lamps to indicate the stop mode */
					if (prev_mode == MODE_FWD)
						lamp_mask = L_FWD;
					else if (prev_mode == MODE_REW)
						lamp_mask = L_REW;
					else if (prev_mode == MODE_PLAY)
						lamp_mask = L_PLAY;
					else
						lamp_mask = 0;

					/* Stop and new lamp mask, diag leds preserved */
					g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_STOP | lamp_mask;

					/* Disable record if active */
					SetTransportMask(0, T_RECH);

					/* Set the reel servos for stop mode */
					ResetStopServo(0);
					SET_SERVO_MODE(MODE_STOP);

			    	prev_mode = MODE_STOP;
					pendstop = MODE_STOP;
					break;

				case MODE_REW:

					/* Disable record if active! */
	        		RecordDisable();

					/* Ignore if already in rew mode */
					if (IS_SERVO_MODE(MODE_REW))
						break;

					/* Indicate shuttle mode active */
					shuttling = 1;

					/* Light the rewind lamp only */
					g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_REW;

					/* REW - stop the capstan servo, disengage brakes,
					 * engage lifters, disengage pinch roller,
					 * disable record.
					 */
					SetTransportMask(T_TLIFT, T_SERVO | T_PROL | T_RECH | T_BRAKE);

					/* Initialize shuttle mode PID values */

				    //Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

					ipid_init(&g_servo.pid,
							 g_sys.shuttle_servo_pgain,     // P-gain
							 g_sys.shuttle_servo_igain,     // I-gain
							 g_sys.shuttle_servo_dgain,     // D-gain
							 PID_TOLERANCE);                 // PID deadband

				    //Semaphore_post(g_semaServo);


       		        // 500 ms delay for tape lifter settling time
        			if (IS_STOPPED())
        				Task_sleep(g_sys.lifter_settle_time);

					/* Set servos to REW mode */
					SET_SERVO_MODE(MODE_REW);

			    	prev_mode = MODE_REW;
					mode = UNDEFINED;
					break;

				case MODE_FWD:

					/* Disable record if active! */
	        		RecordDisable();

					/* Ignore if already in ffwd mode */
					if (IS_SERVO_MODE(MODE_FWD))
						break;

					/* Indicate shuttle mode active */
					shuttling = 1;

					/* Reset all lamps & light fast fwd lamp */
					g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_FWD;

					/* FWD - stop the capstan servo, disengage brakes,
					 * engage lifters, disengage pinch roller,
					 * disable record.
					 */
					SetTransportMask(T_TLIFT, T_SERVO | T_PROL | T_RECH | T_BRAKE);

					 /* Initialize the shuttle tension sensor and velocity PID data */

					//Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

					/* Initialize shuttle mode PID values */
					ipid_init(&g_servo.pid,
							 g_sys.shuttle_servo_pgain,     // P-gain
							 g_sys.shuttle_servo_igain,     // I-gain
							 g_sys.shuttle_servo_dgain,     // D-gain
							 PID_TOLERANCE);                // PID deadband

					//Semaphore_post(g_semaServo);

       		        // 500 ms delay for tape lifter settling time
        			if (IS_STOPPED())
        				Task_sleep(g_sys.lifter_settle_time);

					/* Set servos to FWD mode */
					SET_SERVO_MODE(MODE_FWD);

			    	prev_mode = MODE_FWD;
					mode = UNDEFINED;
					break;

				case MODE_PLAY:

					/* Ignore if already in play mode */
					if (IS_SERVO_MODE(MODE_PLAY))
						break;

					/* upper bit indicates record when starting play mode */
				    playrec = (msg.op & M_RECORD) ? 1 : 0;

					/* Turn on the play lamp */
					g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_PLAY;

					/* Set the reel servos to stop mode initially */
					ResetStopServo(2);
					SET_SERVO_MODE(MODE_STOP);

			    	prev_mode = MODE_PLAY;
					pendstop = MODE_PLAY;
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

            if (!pendstop)
            	continue;

            /* 60 seconds motion stop detect timeout */

            if (++stoptimer >= 1200)
            {
                /* error, the motion didn't stop within timeout period */
                playrec = pendstop = stoptimer = 0;

                /* Stop lamp only, diag leds preserved */
        	    g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_STOP | L_STAT3;

				/* Set reel servos to stop */
        	    ResetStopServo(0);
    		    SET_SERVO_MODE(MODE_STOP);

    		    prev_mode = MODE_STOP;
	    	    mode = UNDEFINED;
                continue;
            }

            /* Blink the stop lamp to indicate stop is pending */

			if ((stoptimer % 6) == 5)
				g_lamp_mask ^= L_STOP;

			/* Process the pending command state */

        	switch(pendstop)
        	{
            	case MODE_STOP:

            		/* Has all motion stopped yet? */
            		if (last_mode != MODE_PLAY)
            		{
            			if (!IS_STOPPED())
            				break;
            		}

                	/* At this point, all motion has stopped from the last command
                	 * that required a pending motion stop state. Here we process
                	 * the final state portion of the previous pending command.
                	 */

            	    if ((last_mode == MODE_FWD) ||
            	        (last_mode == MODE_REW) ||
            	        (last_mode == MODE_PLAY))
            	    {
            	        if ((last_mode == MODE_PLAY) && (g_sys.sysflags & SF_BRAKES_STOP_PLAY))
            	        {
                            /* STOP - Stop capstan servo, engage brakes,
                             * disengage pinch roller, disable record.
                             */
                            SetTransportMask(T_BRAKE, T_SERVO | T_PROL | T_RECH);
           		        }
           		        else
           		        {
                            /* STOP - Stop capstan servo, release brakes,
                             * disengage pinch roller, disable record.
                             */
                            SetTransportMask(0, T_BRAKE | T_SERVO | T_PROL | T_RECH);
           		        }

           		        // 600 ms delay
           		        Task_sleep(600);
           		    }

            		/* Stop lamp only, diag leds preserved */
            		g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_STOP;

            		/* Stop capstan servo, relase brakes, release lifter,
            		 * release pinch roller and end any record mode.
            		 */

            		/* Leave lifter engaged at stop if DIP switch #2 enabled. */
            		if (g_sys.sysflags & SF_LIFTER_AT_STOP)
                		SetTransportMask(T_TLIFT, T_SERVO | T_PROL | T_RECH | T_BRAKE);
            		else
            			SetTransportMask(0, T_SERVO | T_TLIFT | T_PROL | T_RECH | T_BRAKE);

            		/* Leave brakes engaged at stop DIP switch #3 enabled. */
            		if (g_sys.sysflags & SF_BRAKES_AT_STOP)
            		    SetTransportMask(T_BRAKE, 0);

            		/* Tape lifter settling Time */
            		Task_sleep(g_sys.lifter_settle_time);

            		mode = UNDEFINED;
            		shuttling = pendstop = 0;
            		break;

           		case MODE_PLAY:

           			/* Has all motion stopped yet? */
            	    if (last_mode != MODE_PLAY)
            	    {
            	    	if (!IS_STOPPED())
            	    		break;
            	    }

                    /* All motion has stopped, allow 1 second settling
                     * time prior to engaging play after shuttle mode.
                     */
            	    if (shuttling)
            	    {
            	    	shuttling = 0;
            	    	Task_sleep(1000);
            	    }

        		    /* Disengage tape lifters & brakes. */
        		    SetTransportMask(0, T_TLIFT | T_BRAKE);

        		    /* Settling time for tape lifter release */
        		    if (g_sys.sysflags & SF_LIFTER_AT_STOP)
        		    	Task_sleep(g_sys.lifter_settle_time);

        		    /* Play lamp only, diag leds preserved */
        		    g_lamp_mask = (g_lamp_mask & L_LED_MASK) | L_PLAY;

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
        		    ResetPlayServo();

       		        /* [2] Start the reel servos in PLAY mode */
       		        SET_SERVO_MODE(MODE_PLAY);

      		        /* [3] Now start the capstan servo motor */
       		        SetTransportMask(T_SERVO, 0);

       		        /* [4] Enable record if record flag was set */
       		        if (playrec)
       		        {
       		        	RecordEnable();
       		        	playrec = 0;
       		        }

               		mode = UNDEFINED;
           		    pendstop = 0;
           		    break;

   		        default:
           		    //mode = prev_mode = UNDEFINED;
           		    pendstop = 0;
           		    break;
            }
 	    }
    }
}

// End-Of-File
