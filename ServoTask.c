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
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/gpio.h>
#include <ti/drivers/spi.h>
#include <ti/drivers/i2c.h>
#include <ti/drivers/uart.h>

#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/qei.h>

#include <inc/hw_ints.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_gpio.h>

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
#include "TapeTach.h"
#include "MotorDAC.h"
#include "ReelQEI.h"

/* Calculate the tension value from the ADC reading */
#define TENSION(adc)			( (0xFFF - (adc & 0xFFF)) )
#define TENSION_F(adc)			( (2047.0f - (float)adc) )

/* External Global Data */
extern Semaphore_Handle g_semaServo;
extern Semaphore_Handle g_semaTransportMode;

/* Static Function Prototypes */
static void SvcServoHalt(void);
static void SvcServoStop(void);
static void SvcServoPlay(void);
static void SvcServoFwd(void);
static void SvcServoRew(void);

/*****************************************************************************
 * SERVO MODE CONTROL INTERFACE FUNCTIONS (thread safe)
 *****************************************************************************/

void ServoSetMode(uint32_t mode)
{
    uint32_t prev_mode;

    Semaphore_pend(g_semaTransportMode, BIOS_WAIT_FOREVER);

    /* Only the mode number bits */
    mode &= MODE_MASK;

    /* Get the previous mode */
    prev_mode = g_servo.mode_prev;

    /* Update for previous mode state */
    g_servo.mode_prev = g_servo.mode;

    /* Now set the new servo mode state */
    g_servo.mode = mode;

    /* Set dynamic brake state if STOP mode requested and
     * the previous mode was PLAY, FF or REW, otherwise clear it.
     */
    if (mode == MODE_STOP)
        g_servo.stop_brake_state = (prev_mode != MODE_HALT) ? 1 : 0;

    Semaphore_post(g_semaTransportMode);
}

uint32_t ServoGetMode()
{
	uint32_t mode;
	Semaphore_pend(g_semaTransportMode, BIOS_WAIT_FOREVER);
	mode = g_servo.mode & MODE_MASK;
	Semaphore_post(g_semaTransportMode);
	return mode;
}

int32_t IsServoMode(uint32_t mode)
{
	int32_t flag;
	Semaphore_pend(g_semaTransportMode, BIOS_WAIT_FOREVER);
	flag = ((g_servo.mode & MODE_MASK) == (mode & MODE_MASK)) ? 1 : 0;
	Semaphore_post(g_semaTransportMode);
	return flag;
}

int32_t IsServoMotion()
{
	int32_t motion;
	Semaphore_pend(g_semaTransportMode, BIOS_WAIT_FOREVER);
	motion = (g_servo.motion) ? 1 : 0;
	Semaphore_post(g_semaTransportMode);
	return motion;
}

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

#define RADIUS_F(ts, rs)    ( ts / rs )

#define CPR_DIV_2           (1.0f / 2.048f)

#define OFFSET_CALC_PERIOD      500
#define OFFSET_SCALE_F          500.0f      // was 500 for 500 cpr encoders

Void ServoLoopTask(UArg a0, UArg a1)
{
    static void (*jmptab[5])(void) = {
        SvcServoHalt,     /* MODE_HALT */
        SvcServoStop,     /* MODE_STOP */
        SvcServoPlay,     /* MODE_PLAY */
        SvcServoFwd,      /* MODE_FWD  */
        SvcServoRew       /* MODE_REW  */
    };

    /* Initialize servo loop controller data */
    g_servo.mode              = MODE_HALT;
    g_servo.offset_sample_cnt = 0;
    g_servo.offset_null_sum   = 0.0f;
    g_servo.dac_halt_takeup   = 0;
    g_servo.dac_halt_supply   = 0;
	g_servo.play_boost_count  = 0;

    /* Servo's start in halt mode! */
    SET_SERVO_MODE(MODE_HALT);

    /* Initialize the QEI interface and interrupts */
    ReelQEI_initialize();

    /* Initialize tape roller tach timers and interrupt */
    TapeTach_initialize();

    /*** ENTER SERVO LOOP FOREVER ***/

    while (1)
    {
        /* Toggle I/O pin for debug timing measurement*/
        GPIO_write(DTC1200_EXPANSION_PF3, PIN_HIGH);

        /***********************************************************
         * GET THE SUPPLY AND TAKEUP REEL VELOCITY AND DIRECTION
         ***********************************************************/

        /* Read the tape roller tachometer count */
        g_servo.tape_tach = TapeTach_read();

        /* Read the takeup and supply reel motor velocity values */
        g_servo.velocity_supply = (float)QEIVelocityGet(QEI_BASE_SUPPLY);
        g_servo.velocity_takeup = (float)QEIVelocityGet(QEI_BASE_TAKEUP);

        /* Sum the two for current total reel velocity */
        g_servo.velocity = (g_servo.velocity_supply + g_servo.velocity_takeup);

        /* Set the motion active status flag */
        g_servo.motion = (g_servo.velocity > g_sys.vel_detect_threshold) ? 1 : 0;

        /* Read the current direction and make sure both reels are
         * moving and moving in the same direction before changing
         * state to avoid jitter at near stopped conditions.
         */
        int32_t sdir = QEIDirectionGet(QEI_BASE_SUPPLY);
        int32_t tdir = QEIDirectionGet(QEI_BASE_TAKEUP);

        if ((sdir == tdir) && g_servo.motion)
            g_servo.direction = sdir;
        else
        	g_servo.direction = 0;

        /* Read all ADC values which includes the tape tension sensor
         * Step[0] ADC2 - Tension Sensor Arm
         * Step[1] ADC0 - Supply Motor Current Option
         * Step[2] ADC1 - Takeup Motor Current Option
         * Step[3] ADC3 - Expansion Port ADC input option
         * Step[4] Internal CPU temperature sensor
         */
        Board_readADC(g_servo.adc);

        /* Calculate the CPU temp since we read it here anyway */
        g_servo.cpu_temp = (float)g_servo.adc[4];

        /* calculate the tension sensor value */
        g_servo.tsense = TENSION_F(g_servo.adc[0]) * g_sys.tension_sensor_gain;

        /***********************************************************
         * BEGIN REELING RADIUS CALCULATIONS
         ***********************************************************/

        /* Calculate the servo null offset value. The servo null offset
         * is the difference in velocity of the takeup and supply reel.
         * The reel with more pack turns more slowly due to larger radius and
         * visa versa. We calculate the offset as the velocity ratio of the
         * takeup and supply reel velocity by simply dividing the two reel
         * velocity tach values.
         */

        float delta = 0.0f;
        float veldetect = (g_high_speed_flag) ? 40.0f : 20.0f;

        if ((g_servo.velocity_takeup > veldetect) && (g_servo.velocity_supply > veldetect))
        {
            /* Calculate the current reeling radius as tape speed divided by
             * the reel speed. Takeup and supply velocity must not be zero!
             */
            g_servo.radius_takeup = RADIUS_F(g_servo.tape_tach, g_servo.velocity_takeup) * g_sys.reel_radius_gain;
            g_servo.radius_supply = RADIUS_F(g_servo.tape_tach, g_servo.velocity_supply) * g_sys.reel_radius_gain;

            /* Calculate the delta factor between the two reel motor tach's */
            if (g_servo.velocity_takeup > g_servo.velocity_supply)
            {
                delta = ((g_servo.velocity_takeup * OFFSET_SCALE_F) / g_servo.velocity_supply) - OFFSET_SCALE_F;
            }
            else if (g_servo.velocity_supply > g_servo.velocity_takeup)
            {
                delta = ((g_servo.velocity_supply * OFFSET_SCALE_F) / g_servo.velocity_takeup) - OFFSET_SCALE_F;
            }
            else
            {
                delta = 0.0f;
            }

            if (delta > 1000.0f)
                delta = 1000.0f;

            /* Accumulate the delta for averaging over 1 second */
            g_servo.offset_null_sum += delta;

            ++g_servo.offset_sample_cnt;

            if (g_servo.offset_sample_cnt >= OFFSET_CALC_PERIOD)
            {
                float offset = g_servo.offset_null_sum / (float)OFFSET_CALC_PERIOD;

                /* Calculate the averaged null offset value */
                g_servo.offset_null = offset * g_sys.reel_offset_gain;

                /* Reset the accumulator and sample counter */
                g_servo.offset_null_sum   = 0.0f;
                g_servo.offset_sample_cnt = 0;
            }

            /* Now store the calculated offset for each reel motor that is added
             * or subtracted to compensate the torque applied for the current
             * reel radius offset. These values are used to adjust the torque on
             * each reel based on the reel velocity to compensate for the constantly
             * changing reel hub radius.
             */

            if (g_sys.reel_offset_gain <= 0.0f)
            {
                /* for debugging & aligning system */
                g_servo.offset_supply = 0.0f;
                g_servo.offset_takeup = 0.0f;
            }
            else
            {
                if (g_servo.velocity_takeup > g_servo.velocity_supply)
                {
                    /* TAKEUP reel is turning faster than the SUPPLY reel!
                     * ADD to the TAKEUP reel torque.
                     * SUBTRACT from the SUPPLLY reel torque.
                     */
                    g_servo.offset_supply =  (g_servo.offset_null);
                    g_servo.offset_takeup = -(g_servo.offset_null);
                }
                else if (g_servo.velocity_supply > g_servo.velocity_takeup)
                {
                    /* SUPPLY reel is turning faster than the TAKE-UP reel!
                     * ADD to the SUPPLY reel torque.
                     * SUBTRACT from the TAKEUP reel torque.
                     */
                    g_servo.offset_supply = -(g_servo.offset_null);
                    g_servo.offset_takeup =  (g_servo.offset_null);
                }
                else
                {
                    g_servo.offset_supply = 0.0f;
                    g_servo.offset_takeup = 0.0f;
                }
            }
        }

        /**********************************************
         * DISPATCH TO THE CURRENT SERVO MODE HANDLER
         **********************************************/

        (*jmptab[ServoGetMode()])();

        /* Toggle I/O pin for debug timing measurement*/
        GPIO_write(DTC1200_EXPANSION_PF3, PIN_LOW);

        /* 500 Hz (2ms) sample rate */
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
    MotorDAC_write((float)g_servo.dac_halt_supply, (float)g_servo.dac_halt_takeup);
}

//*****************************************************************************
// PLAY SERVO - This function handles play mode servo logic and is
// called at periodic intervals at the sample frequency specified
// by the timer interrupt.
//*****************************************************************************

static void SvcServoPlay(void)
{
    float dac_s;
    float dac_t;
    float target_velocity;
    float cv = 0.0f;

    /* Calculate the "reeling radius" for each reel */
    //float rad_t = g_servo.radius_takeup;
    //float rad_s = g_servo.radius_supply;

    /* Play acceleration boost state? */

    Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

    if (g_servo.play_boost_count)
    {
        --g_servo.play_boost_count;

        /* Boost status LED on */
        g_lamp_mask |= L_STAT3;

        /* Get the PID current CV value based on the velocity */

        target_velocity = (float)g_servo.play_boost_end;

        cv = fpid_calc(
                &g_servo.pid_play,      /* play boost PID accumulator      */
        		target_velocity,        /* desired velocity for play speed */
				g_servo.tape_tach);     /* current tape roller velocity    */

        /* DECREASE SUPPLY Motor Torque */
        dac_s = (((float)g_servo.play_supply_tension + g_servo.tsense)) + g_servo.offset_supply;
        /* INCREASE TAKEUP Motor Torque */
        dac_t = ((float)g_servo.play_takeup_tension + cv) + g_servo.offset_takeup;

        if (!g_servo.play_boost_count)
        	g_lamp_mask &= ~(L_STAT3);

        /* Has tape roller tach reached the correct speed yet? */
        if (cv <= 0.0f)
        {
            /* End play boost mode */
            g_servo.play_boost_count = 0;
            /* Turn off boost status LED */
            g_lamp_mask &= ~(L_STAT3);
        }

        // DEBUG
        g_servo.db_cv    = cv;
        g_servo.db_error = g_servo.pid_play.error;
        g_servo.db_debug = target_velocity;
    }
    else
    {
        /* Calculate the SUPPLY boost torque */
        dac_s = (g_servo.play_supply_tension + g_servo.tsense) + g_servo.offset_supply;

        /* Calculate the TAKEUP boost torque */
        dac_t = (g_servo.play_takeup_tension + g_servo.tsense) + g_servo.offset_takeup;
    }

    Semaphore_post(g_semaServo);

    /* Clamp the DAC values within range if needed */

    DAC_CLAMP(dac_s, 0.0f, DAC_MAX_F);
    DAC_CLAMP(dac_t, 0.0f, DAC_MAX_F);

    /* Set the servo DAC levels */

    MotorDAC_write(dac_s, dac_t);
}

//*****************************************************************************
// STOP SERVO - This function handles dynamic braking for stop mode to
// null all motion by applying opposing torque force to each reel. This works
// in either tape direction and at velocity. The amount of braking torque
// applied at any given velocity is controlled by the 'stop_brake_torque'
// configuration parameter.
//*****************************************************************************

static void SvcServoStop(void)
{
    float dac_s;
    float dac_t;
    float braketorque = 0.0f;

    /*** Calculate the dynamic braking torque from velocity ***/

	Semaphore_pend(g_semaTransportMode, BIOS_WAIT_FOREVER);

	/* Is dynamic braking enabled for STOP servo mode? */
	if (g_servo.stop_brake_state)
	{
		/* Has velocity reached zero yet? */
	    if (g_servo.velocity <= (float)g_sys.vel_detect_threshold)
	    {
	    	/* Disable dynamic brake state */
	        g_servo.stop_brake_state = 0;
	    }
	    else
	    {
    		/* Calculate dynamic braking torque from current velocity */
	    	if (g_servo.stop_brake_state > 1)
	    		braketorque = (g_servo.velocity * 5.0f);
	    	else
	    		braketorque = (float)g_sys.stop_brake_torque - (g_servo.velocity * CPR_DIV_2);

	        /* Check for brake torque limit overflow */
	        if (braketorque < 0.0f)
	        	braketorque = (float)g_sys.stop_brake_torque;

	        if (braketorque > (float)g_sys.stop_brake_torque)
	        	braketorque = (float)g_sys.stop_brake_torque;
	    }
	}

	Semaphore_post(g_semaTransportMode);

	/* Save brake torque for debug purposes */
    g_servo.stop_torque_takeup = braketorque;
    g_servo.stop_torque_supply = braketorque;

	/*
	 * DYNAMIC BRAKING: Apply the braking torque required to null motion.
	 */

    if (g_servo.direction == TAPE_DIR_FWD)
    {
        /* FORWARD DIR MOTION - increase supply torque */
        dac_s = ((((float)g_sys.stop_supply_tension + g_servo.tsense) + braketorque) + g_servo.offset_supply);
        /* decrease takeup torque */
        dac_t = ((((float)g_sys.stop_takeup_tension + g_servo.tsense) - braketorque) + g_servo.offset_takeup);
    }
    else if (g_servo.direction == TAPE_DIR_REW)
    {
        /* REWIND DIR MOTION - decrease supply torque */
        dac_s = ((((float)g_sys.stop_supply_tension + g_servo.tsense) - braketorque) + g_servo.offset_supply);
        /* increase takeup torque */
        dac_t = ((((float)g_sys.stop_takeup_tension + g_servo.tsense) + braketorque) + g_servo.offset_takeup);
    }
    else
    {
    	/* error, no motion? */
        dac_s = (((float)g_sys.stop_supply_tension + g_servo.tsense) + g_servo.offset_supply);
        dac_t = (((float)g_sys.stop_takeup_tension + g_servo.tsense) + g_servo.offset_takeup);
    }

    /* Safety Clamps */

    DAC_CLAMP(dac_s, 0.0f, DAC_MAX_F);
    DAC_CLAMP(dac_t, 0.0f, DAC_MAX_F);

    /* Set the DAC levels to the servos */

    MotorDAC_write(dac_s, dac_t);
}

//*****************************************************************************
// FWD SERVO - This function handles forward mode servo logic and is
// called at periodic intervals at the sample frequency specified
// by the timer interrupt.
//*****************************************************************************

static void SvcServoFwd(void)
{
    float dac_s;
    float dac_t;

    static float cv = 0.0f;

    /* Auto reduce velocity when nearing end of the
     * reel based on the null offset value.
     */

    float target_velocity = (float)g_servo.shuttle_velocity;

    if (g_sys.shuttle_autoslow_velocity)
    {
        if (fabsf(g_servo.offset_takeup) >= (float)g_sys.shuttle_autoslow_offset)
        {
        	/* Shuttling towards tail of reel? */
        	if ((g_servo.velocity_supply > g_servo.velocity_takeup) && (g_servo.velocity >= (g_sys.shuttle_velocity - 10.0f)))
        	{
				float slow_velocity = (float)g_sys.shuttle_autoslow_velocity;

				if ((g_servo.velocity >= slow_velocity) && (g_servo.velocity > 300.0f))
				{
					target_velocity = slow_velocity;
					g_servo.shuttle_velocity = target_velocity;
				}
        	}
        }
    }

    /* Get the PID current CV value based on the velocity */

    Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

    cv = fpid_calc(
        &g_servo.pid_shuttle,	/* PID accumulator  */
        target_velocity,		/* desired velocity */
        g_servo.velocity   		/* current velocity */
        );

    /* If shuttle direction changed and we were over speed,
     * then don't allow CV to be negative as this would
     * cause speed to *increase* when direction changed!
     * So, we force it positive to begin dynamic braking
     * action instead.
     */

    if ((g_servo.mode_prev == MODE_REW) && (cv < 0.0f))
        cv = fabs(cv);

    Semaphore_post(g_semaServo);

    /* Back tension compensates for decreasing motor torque as the motors
     * gain velocity and free wheel. Initially the motor current is high
     * as torque is first applied, but the current drops as the motor
     * gains velocity and free wheels at the target velocity.
     */

    float holdback = g_servo.velocity * g_servo.radius_supply * g_sys.shuttle_holdback_gain;

    // DEBUG
    g_servo.db_cv    = cv;
    g_servo.db_error = g_servo.pid_shuttle.error;
    g_servo.db_debug = target_velocity;
    g_servo.holdback = holdback;

    /* DECREASE SUPPLY Motor Torque */
    dac_s = (((float)g_sys.shuttle_supply_tension + holdback + g_servo.tsense) - cv) + g_servo.offset_supply;

    /* INCREASE TAKEUP Motor Torque */
    dac_t = (((float)g_sys.shuttle_takeup_tension + g_servo.tsense) + cv) + g_servo.offset_takeup;

    /* Safety Clamp */
    DAC_CLAMP(dac_s, 0.0f, DAC_MAX_F);
    DAC_CLAMP(dac_t, 0.0f, DAC_MAX_F);

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
    float dac_s;
    float dac_t;

    static float cv = 0.0f;

    /* Auto reduce velocity when nearing end of the
     * reel based on the null offset value.
     */

    float target_velocity = (float)g_servo.shuttle_velocity;

    if (g_sys.shuttle_autoslow_velocity)
    {
        if (fabsf(g_servo.offset_supply) >= (float)g_sys.shuttle_autoslow_offset)
        {
        	/* Shuttling towards head of reel? */
        	if ((g_servo.velocity_supply < g_servo.velocity_takeup) && (g_servo.velocity >= (g_sys.shuttle_velocity - 10.0f)))
        	{
				float slow_velocity = (float)g_sys.shuttle_autoslow_velocity;

				if (g_servo.velocity >= slow_velocity)
				{
					target_velocity = slow_velocity;
					g_servo.shuttle_velocity = target_velocity;
				}
        	}
        }
    }

    /* Get the PID current CV value based on the velocity */

    Semaphore_pend(g_semaServo, BIOS_WAIT_FOREVER);

    cv = fpid_calc(
        &g_servo.pid_shuttle,   /* PID accumulator  */
        target_velocity,		/* desired velocity */
        g_servo.velocity   		/* current velocity */
        );

    /* If shuttle direction changed and we were over speed,
     * then don't allow CV to be negative as this would
     * cause speed to *increase* when direction changed!
     * So, we force it positive to begin dynamic braking
     * action instead.
     */

    if ((g_servo.mode_prev == MODE_FWD) && (cv < 0.0f))
        cv = fabs(cv);

    Semaphore_post(g_semaServo);

    /* Back tension compensates for decreasing motor torque as the motors
     * gain velocity and free wheel. Initially the motor current is high
     * as torque is first applied, but the current drops as the motor
     * gains velocity and free wheels at the target velocity.
     */

    float holdback = g_servo.velocity * g_servo.radius_takeup * g_sys.shuttle_holdback_gain;

    // DEBUG
    g_servo.db_cv    = cv;
    g_servo.db_error = g_servo.pid_shuttle.error;
    g_servo.db_debug = target_velocity;
    g_servo.holdback = holdback;

    /* INCREASE SUPPLY Motor Torque */
    dac_s = (((float)g_sys.shuttle_supply_tension + g_servo.tsense) + cv) + g_servo.offset_supply;

    /* DECREASE TAKEUP Motor Torque */
    dac_t = (((float)g_sys.shuttle_takeup_tension + holdback + g_servo.tsense) - cv) + g_servo.offset_takeup;

    /* Safety clamp */
    DAC_CLAMP(dac_s, 0.0f, DAC_MAX_F);
    DAC_CLAMP(dac_t, 0.0f, DAC_MAX_F);

    /* Set the servo DAC levels */
    MotorDAC_write(dac_s, dac_t);
}

/* End-Of-File */
