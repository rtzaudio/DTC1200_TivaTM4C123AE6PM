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
#include "Globals.h"
#include "ServoTask.h"

#include "IOExpander.h"
#include "TransportTask.h"
#include "TachTimer.h"
#include "MotorDAC.h"

/* Calculate the tension value from the ADC reading */
#define TENSION(adc, g)		((0xFFF - (adc & 0xFFF)) >> g)

#define OFFSET_SCALE		500
#define OFFSET_CALC_PERIOD	500


/* Static Servo Mode Prototypes */
static void SvcServoHalt(void);
static void SvcServoStop(void);
static void SvcServoPlay(void);
static void SvcServoFwd(void);
static void SvcServoRew(void);

/*****************************************************************************
 * MAIN SERVO LOOP CONTROLLER TASK
 *
 * This is the highest priority system task for the reel motor servo loop.
 * This tick handler gets called 500 times per second for the servo loop.
 * The appropriate servo loop mode handler is called for each of the
 * Halt, Stop, Play, Forward and Rewind transport sevo loop modes.
 * Each transport mode of operation requires a different servo loop.
 *
 *****************************************************************************/
Void ServoLoopTask(UArg a0, UArg a1)
{
    static void (*jmptab[5])(void) = {
        SvcServoHalt,     /* MODE_HALT */
        SvcServoStop,     /* MODE_STOP */
        SvcServoPlay,     /* MODE_PLAY */
        SvcServoFwd,      /* MODE_FWD  */
        SvcServoRew       /* MODE_REW  */
    };

    while (1)
    {
		/* Toggle I/O pin for debug timing measurement*/
		//GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_PIN_0);

		/***********************************************************
		 * GET THE SUPPLY AND TAKEUP REEL VELOCITY AND DIRECTION
		 ***********************************************************/

		/* Read the tape roller tachometer count */
		g_servo.tape_tach = ReadTapeTach();

		/* Read the takeup and supply reel motor velocity values */
		g_servo.velocity_supply = (long)QEIVelocityGet(QEI_BASE_SUPPLY);
		g_servo.velocity_takeup = (long)QEIVelocityGet(QEI_BASE_TAKEUP);

		/* Calculate the current total reel velocity. */
		g_servo.velocity = g_servo.velocity_supply + g_servo.velocity_takeup;

		/* Read the current direction and make sure both reels are
		 * moving and moving in the same direction before changing
		 * state to avoid jitter at near stopped conditions.
		 */
		long sdir = QEIDirectionGet(QEI_BASE_SUPPLY);
		long tdir = QEIDirectionGet(QEI_BASE_TAKEUP);

		if ((sdir == tdir) && (g_servo.velocity > g_sys.velocity_detect))
			g_servo.direction = sdir;

		/* Read all ADC values which includes the tape tension sensor
		 * Step[0] ADC2 - Tension Sensor Arm
		 * Step[1] ADC0 - Supply Motor Current Option
		 * Step[2] ADC1 - Takeup Motor Current Option
		 * Step[3] ADC3 - Expansion Port ADC input option
		 * Step[4] Internal CPU temperature sensor
		 */
		Board_readADC(g_servo.adc);

		/* calculate the tension sensor value */
		g_servo.tsense = TENSION(g_servo.adc[0], 2);

		/***********************************************************
		 * BEGIN CALCULATIONS FROM DATA SAMPLE
		 ***********************************************************/

		/* Calculate the servo null offset value. The servo null offset
		 * is the difference in velocity of the takeup and supply reel.
		 * The reel with more pack turns more slowly due to larger radius and
		 * visa versa. We calculate the offset as the velocity ratio of the
		 * takeup and supply reel velocity by simply dividing the two reel
		 * velocity tach values.
		 */
		if ((g_servo.velocity_takeup > g_sys.velocity_detect) && (g_servo.velocity_supply > g_sys.velocity_detect))
		{
			long delta;

			if (g_servo.velocity_takeup > g_servo.velocity_supply)
				delta = ((g_servo.velocity_takeup * OFFSET_SCALE) / g_servo.velocity_supply) - OFFSET_SCALE;
			else if (g_servo.velocity_supply > g_servo.velocity_takeup)
				delta = ((g_servo.velocity_supply * OFFSET_SCALE) / g_servo.velocity_takeup) - OFFSET_SCALE;
			else
				delta = 0;

			if (delta < 0)
				delta = 0;

			/* Accumulate the null offset value for averaging */
			g_servo.offset_null_sum += delta;

			if (g_servo.offset_sample_cnt++ >= OFFSET_CALC_PERIOD)
			{
				/* Calculate the averaged null offset value */
				g_servo.offset_null = (g_servo.offset_null_sum / OFFSET_CALC_PERIOD) >> g_sys.null_offset_gain;

				//ConsolePrintf("off=%d\r\n", g_servo.offset_null);

				/* Reset the accumulator and sample counter */
				g_servo.offset_null_sum = 0;
				g_servo.offset_sample_cnt = 0;
			}
		}

		/* Calculate and scale the null offset for each reel servo with a
		 * gain factor. These values are used to adjust the torque on each
		 * reel based on the reel velocity to compensate for the constantly
		 * changing reel hub radius.
		 */

		if (g_sys.null_offset_gain <= 0)
		{
			/* for debugging & aligning system */
			g_servo.offset_supply = 0;
			g_servo.offset_takeup = 0;
		}
		else if ((g_servo.velocity_takeup > g_sys.velocity_detect) && (g_servo.velocity_supply > g_sys.velocity_detect))
		{
			long offset;

			if (g_servo.velocity_takeup > g_servo.velocity_supply)
			{
				/* TAKEUP reel is turning faster than the SUPPLY reel!
				 * ADD to the TAKEUP reel torque.
				 * SUBTRACT from the SUPPLLY reel torque.
				 */
				offset = g_servo.offset_null;
				g_servo.offset_supply = (offset);
				g_servo.offset_takeup =  -(offset);
			}
			else if (g_servo.velocity_supply > g_servo.velocity_takeup)
			{
				/* SUPPLY reel is turning faster than the TAKE-UP reel!
				 * ADD to the SUPPLY reel torque.
				 * SUBTRACT from the TAKEUP reel torque.
				 */
				offset = g_servo.offset_null;
				g_servo.offset_supply = -(offset);
				g_servo.offset_takeup = (offset);
			}
			else
			{
				/* reels at same velocity! */
				g_servo.offset_supply = 0;
				g_servo.offset_takeup = 0;
				g_servo.offset_null = offset;
			}
		}

		/**********************************************
		 * DISPATCH TO THE CURRENT SERVO MODE HANDLER
		 **********************************************/

		(*jmptab[g_cmode & MODE_MASK])();

		/* Toggle I/O pin for debug timing measurement*/
		//GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);

    	Task_sleep(2);
    }
}

//*****************************************************************************
// HALT SERVO - This function halts all reel servo torque and is
// called at periodic intervals at the sample frequency specified
// by the timer interrupt. The global variables g_halt_dac_takeup
// and g_halt_dac_supply are normally set to zero, except during
// diagnostic mode where they are used to ramp the DAC outputs.
//*****************************************************************************

static void SvcServoHalt(void)
{
    MotorDAC_write(g_servo.dac_halt_supply, g_servo.dac_halt_takeup);
}

//*****************************************************************************
// STOP SERVO - This function handles dynamic braking for stop mode to
// null all motion by applying opposing torque force to each reel. This works
// in either tape direction and at velocity. The amount of braking torque
// applied at any given velocity is controlled by the 'stop_brake_torque'
// configuration parameter.
//*****************************************************************************

#if (QE_TIMER_PERIOD > 500000)
#define BRAKE_THRESHOLD_VEL		250
#else
#define BRAKE_THRESHOLD_VEL		25
#endif

static void SvcServoStop(void)
{
    long dac_t;
    long dac_s;

    // RPM(g_servo.velocity_supply);
    // RPM(g_servo.velocity_takeup);

    /*** Calculate the braking torque for each reel ***/

    if (g_servo.velocity <= g_sys.velocity_detect)
    {
        /* No motion, set stop mode tension levels */
        dac_s = g_sys.stop_supply_tension + g_servo.tsense + g_servo.offset_supply;
        dac_t = g_sys.stop_takeup_tension + g_servo.tsense + g_servo.offset_takeup;

        g_servo.stop_null_supply = g_servo.stop_null_takeup = 0;

        /* Reset the brake state if motion starts back up */
    	IArg key = Gate_enterModule();
		{
			g_servo.brake_state = 0;
			g_servo.brake_torque = 0;
		}
		Gate_leaveModule(key);
    }
    else
    {
    	/*
    	 * STATE-0 : ramp up brake torque
    	 */

    	if (g_servo.brake_state == 0)
    	{
        	IArg key = Gate_enterModule();
    		{
    			if (g_servo.velocity <= BRAKE_THRESHOLD_VEL)
    				g_servo.brake_torque += 5;
    			else
    				g_servo.brake_torque += 10;

				if (g_servo.brake_torque >= g_sys.stop_brake_torque)
					g_servo.brake_state = 1;

				if (g_servo.velocity <= BRAKE_THRESHOLD_VEL)
					g_servo.brake_state = 1;
    		}
    		Gate_leaveModule(key);
    	}

    	/*
    	 * STATE-1 : keep applying brake torque or ramp down torque
    	 *           to release brake if at threshold velocity.
    	 */

    	if (g_servo.brake_state == 1)
    	{
			if (g_servo.velocity <= BRAKE_THRESHOLD_VEL)
			{
		    	IArg key = Gate_enterModule();
	    		{
					if (g_servo.brake_torque)
						g_servo.brake_torque -= 5;

					if (!g_servo.brake_torque)
						g_servo.brake_state = 0;
	    		}
	    		Gate_leaveModule(key);
			}

			g_servo.stop_null_supply = g_servo.brake_torque;
			g_servo.stop_null_takeup = g_servo.brake_torque;
    	}

    	if (g_servo.brake_state == 2)
    	{
    		g_servo.stop_null_supply = g_servo.stop_null_takeup = g_servo.brake_torque = 0;
    	}

    	/*
    	 * DYNAMIC BRAKING: Apply the braking torque required to null motion.
    	 */

        if (g_servo.direction == TAPE_DIR_FWD)
        {
            /* FORWARD DIR MOTION - increase supply torque */
            dac_s = ((g_sys.stop_supply_tension + g_servo.tsense) + g_servo.stop_null_supply) + g_servo.offset_supply;
            /* decrease takeup torque */
            dac_t = ((g_sys.stop_takeup_tension + g_servo.tsense) - g_servo.stop_null_takeup) + g_servo.offset_takeup;
        }
        else if (g_servo.direction == TAPE_DIR_REW)
        {
            /* REWIND DIR MOTION - decrease supply torque */
            dac_s = ((g_sys.stop_supply_tension + g_servo.tsense) - g_servo.stop_null_supply) + g_servo.offset_supply;
            /* increase takeup torque */
            dac_t = ((g_sys.stop_takeup_tension + g_servo.tsense) + g_servo.stop_null_takeup) + g_servo.offset_takeup;
        }
        else
        {
            /* error, no motion? */
            dac_s = (g_sys.stop_supply_tension + g_servo.tsense) + g_servo.offset_supply;
            dac_t = (g_sys.stop_takeup_tension + g_servo.tsense) + g_servo.offset_takeup;
        }
    }

    /* Safety Clamps */

    DAC_CLAMP(dac_s,
              g_sys.stop_min_torque,
              g_sys.stop_max_torque);

    DAC_CLAMP(dac_t,
              g_sys.stop_min_torque,
              g_sys.stop_max_torque);

    /* Set the DAC levels to the servos */

    MotorDAC_write(dac_s, dac_t);
}

//*****************************************************************************
// PLAY SERVO - This function handles play mode servo logic and is
// called at periodic intervals at the sample frequency specified
// by the timer interrupt.
//*****************************************************************************

#if (QE_TIMER_PERIOD > 500000)
#define RADIUS(ts, rs)	( ((ts*10)/((rs)+1) >> 1 ) )
#else
#define RADIUS(ts, rs)	( (ts)/((rs)+1) )
#endif

static void SvcServoPlay(void)
{
    long dac_s;
    long dac_t;
    long rad_t;
	long rad_s;

	/* Calculate the "reeling radius" for each reel */
	rad_t = RADIUS(g_servo.tape_tach, g_servo.velocity_takeup);
	rad_s = RADIUS(g_servo.tape_tach, g_servo.velocity_supply);

   	/* Play acceleration boost state? */

   	if (g_servo.play_boost_time)
   	{
		g_servo.play_boost_count += 1;

   		dac_t = (g_servo.play_boost_start << 1) / ((g_servo.velocity_takeup / 8) + 1);

   		dac_s = (g_servo.play_supply_tension + g_servo.offset_supply) + TENSION(g_servo.adc[0], 0);

   	    /* Boost status LED on */
   		g_lamp_mask |= L_STAT3;

   		if (g_servo.play_boost_time >= g_servo.play_boost_step)
   		{
			g_servo.play_boost_time -= g_servo.play_boost_step;
#if 1
			/* End boost if we're at the desired speed */
			if (g_servo.play_boost_end)
			{
				if (g_servo.tape_tach >= g_servo.play_boost_end)
				{
					g_servo.play_boost_time = 0;

					/* Turn off boost status LED */
					g_lamp_mask &= ~(L_STAT3);
				}
			}
#endif
   		}
   		else
   		{
   	    	g_servo.play_boost_time = 0;

   	    	/* Turn off boost status LED */
   	   		g_lamp_mask &= ~(L_STAT3);
   		}
   	}
   	else
   	{
   		/* Calculate the SUPPLY Torque & Safety clamp */
   		dac_s = ((g_servo.play_supply_tension * rad_s) / g_servo.play_tension_gain) + g_servo.tsense;

   		/* Calculate the TAKEUP Torque & Safety clamp */
   		dac_t = ((g_servo.play_takeup_tension * rad_t) / g_servo.play_tension_gain) + g_servo.play_takeup_tension;
   	}

   	// DEBUG
    g_servo.db_debug = rad_t;

    /* Play Mode Diagnostic Data Capture */

#if (CAPDATA_SIZE > 0)

    if (g_capdata_count < CAPDATA_SIZE)
    {
    	g_capdata[g_capdata_count].dac_supply = dac_s;
    	g_capdata[g_capdata_count].dac_takeup = dac_t;
    	g_capdata[g_capdata_count].vel_supply = g_servo.velocity_supply;
    	g_capdata[g_capdata_count].vel_takeup = g_servo.velocity_takeup;
    	g_capdata[g_capdata_count].rad_supply = rad_t;
    	g_capdata[g_capdata_count].rad_takeup = rad_s;
    	g_capdata[g_capdata_count].tape_tach  = g_servo.tape_tach;
    	g_capdata[g_capdata_count].tension    = g_servo.tsense;
    	++g_capdata_count;
    }
#endif

    /* Clamp the DAC values within range if needed */

    DAC_CLAMP(dac_s,
              g_sys.play_min_torque,
              g_sys.play_max_torque);

    DAC_CLAMP(dac_t,
              g_sys.play_min_torque,
              g_sys.play_max_torque);

    /* Set the servo DAC levels */

    MotorDAC_write(dac_s, dac_t);
}

//*****************************************************************************
// FWD SERVO - This function handles forward mode servo logic and is
// called at periodic intervals at the sample frequency specified
// by the timer interrupt.
//*****************************************************************************

static void SvcServoFwd(void)
{
    long cv;
    long dac_s;
    long dac_t;
    long target_velocity;

    /* Auto reduce velocity when nearing end of the
     * reel based on the null offset value.
     */

    target_velocity = g_sys.shuttle_velocity;

    if (g_sys.shuttle_slow_velocity > 0)
    {
    	if (g_servo.offset_takeup >= g_sys.shuttle_slow_offset)
    	{
    		if (g_servo.velocity >= g_sys.shuttle_slow_velocity)
    			target_velocity = g_sys.shuttle_slow_velocity;
    	}
    }

    /* Get the PID current CV value based on the velocity */

    cv = ipid_calc(
            &g_servo.pid,      		/* PID accumulator  */
            target_velocity,        /* desired velocity */
            g_servo.velocity		/* current velocity */
            );

    // DEBUG
    g_servo.db_cv    = cv;
    g_servo.db_error = g_servo.pid.error;
    g_servo.db_debug = target_velocity;

    /* DECREASE SUPPLY Torque & Safety clamp */

    dac_s = ((g_sys.shuttle_supply_tension + g_servo.tsense) - cv) + g_servo.offset_supply;

    DAC_CLAMP(dac_s,
              g_sys.shuttle_min_torque,
              g_sys.shuttle_max_torque);

    /* INCREASE TAKEUP Torque & Safety clamp */

    dac_t = ((g_sys.shuttle_takeup_tension + g_servo.tsense) + cv) + g_servo.offset_takeup;

    DAC_CLAMP(dac_t,
              g_sys.shuttle_min_torque,
              g_sys.shuttle_max_torque);

    /* Set the servo DAC levels */

    MotorDAC_write(dac_s, dac_t);
}

//*****************************************************************************
// REW SERVO - This function handles rewind mode servo logic and is
// called at periodic intervals at the sample frequency specified
// by the timer interrupt.
//*****************************************************************************

static void SvcServoRew(void)
{
    long cv;
    long dac_s;
    long dac_t;
    long target_velocity;

    /* Auto reduce velocity when nearing end of the
     * reel based on the null offset value.
     */

	target_velocity = g_sys.shuttle_velocity;

    if (g_sys.shuttle_slow_velocity > 0)
    {
    	if (g_servo.offset_supply >= g_sys.shuttle_slow_offset)
    	{
    		if (g_servo.velocity >= g_sys.shuttle_slow_velocity)
    			target_velocity = g_sys.shuttle_slow_velocity;
    	}
    }

    /* Get the PID current CV value based on the velocity */

    cv = ipid_calc(
            &g_servo.pid,      			/* PID accumulator  */
            target_velocity,         	/* desired velocity */
            g_servo.velocity			/* current velocity */
            );

    // DEBUG
    g_servo.db_cv    = cv;
    g_servo.db_error = g_servo.pid.error;
    g_servo.db_debug = target_velocity;

    /* INCREASE SUPPLY Torque & Safety clamp */

    dac_s = ((g_sys.shuttle_supply_tension + g_servo.tsense) + cv) + g_servo.offset_supply;

    DAC_CLAMP(dac_s,
              g_sys.shuttle_min_torque,
              g_sys.shuttle_max_torque);

    /* DECREASE TAKEUP Torque & Safety clamp */

    dac_t = ((g_sys.shuttle_takeup_tension + g_servo.tsense) - cv) + g_servo.offset_takeup;

    DAC_CLAMP(dac_t,
              g_sys.shuttle_min_torque,
              g_sys.shuttle_max_torque);

    /* Set the servo DAC levels */

    MotorDAC_write(dac_s, dac_t);
}

