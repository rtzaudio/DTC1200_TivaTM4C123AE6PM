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

#define DEBUG	1

/* Standard includes */
#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

/* Standard Stellaris includes */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_ssi.h"
#include "inc/hw_i2c.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/adc.h"
#include "driverlib/can.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/ssi.h"
#include "driverlib/i2c.h"
#include "driverlib/qei.h"
#include "driverlib/interrupt.h"
#include "driverlib/pwm.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"

#include "pid.h"

#include "Board.h"

/*** Global Constants ******************************************************/

/* version info */
#define FIRMWARE_VER        2           /* firmware version */
#define FIRMWARE_REV        0        	/* firmware revision */

#define MAGIC               0xCEB0FACE  /* magic number for EEPROM data */
#define MAKEREV(v, r)       ((v << 16) | (r & 0xFFFF))

#define MS(msec)  			( msec / portTICK_RATE_MS )

#define UNDEFINED           (-1)

#define TIMEOUT_SPI			1000

/*** Hardware Constants ****************************************************/

/*
 * Tape moves at 30 IPS in high speed and 15 IPS in low speed. We want to
 * sample and correct at 0.3" intervals so our sample rate needs to
 * be 100 Hz (30/0.3 = 100). The timer delay period required in
 * milliseconds is 1/Hz * 1000. Therefore 1/100*1000 = 10ms. Therefore,
 * if we to correct at 0.15" intervals, the sample period would be 5ms.
 */

//#define SAMPLE_PERIOD_30_IPS	10
//#define SAMPLE_PERIOD_15_IPS	20

/* This macro is used to calculate the RPM of the motor based on velocity.
 * Our encoders are US Digital Model H1-360-I with 360 cycles per revolution.
 * We configure the QEI to capture edges on both signals and maintain an
 * absolute angular position by resetting on index pulses. So, our 360 CPR
 * encoder at four edges per line, gives us 1440 pulses per revolution.
 *
 * The period of the timer is configurable by specifying the load value
 * for the timer in the QEILOAD register. We can calculate RPM with
 * the following equation:
 *
 *	RPM = (clock * (2 ^ VelDiv) * Speed * 60) / (Load * PPR * Edges)
 *
 * For our case, consider a motor running at 600 rpm. A 360 pulse per
 * revolution quadrature encoder is attached to the motor, producing 1440
 * phase edges per revolution. With a velocity pre-divider of ÷1
 * (VelDiv set to 0) and clocking on both PhA and PhB edges, this results
 * in 14,400 pulses per second (the motor turns 10 times per second).
 * If the timer were clocked at 50,000,000 Hz, and the load value was
 * 12,500,000 (1/4 of a second), it would count 14400 pulses per update.
 *
 *	RPM = (50,000,000 * 1 * s * 60) / (500,000 * 360 * 4) = 600 rpm
 *	RPM = (100 * s) / 24 = 600 rpm
 *	RPM = (25 * s) / 6 = 600 rpm
 *
 *	RPM = (50,000,000 * 1 * s * 60) / (5,000,000 * 360 * 4) = 600 rpm
 *	RPM = (10 * s) / 24 = 600 rpm
 */


#define QE_EDGES_PER_REV	(360 * 4)	/* 360 * 4 for four quad encoder edges */
#define QE_TIMER_PERIOD		500000		/* period of 500,000 is 10ms at 50MHz  */

/* Calculate RPM from the velocity value */
#if (QE_TIMER_PERIOD == 500000)
	#define RPM(t)				((25 * t) / 6)
#elif (QE_TIMER_PERIOD == 5000000)
	#define RPM(t)				((10 * t) / 24)
#else
	#pragma(error, "Invalid QE_TIMER_PERIOD Specified!")
#endif

/*
 * Hardware Constants
 */

#define DAC_MIN             0           	/* zero scale dac setting  */
#define DAC_MAX             0x03FF        	/* 10-bit full scale DAC   */

#define ADC_MIN             0           	/* zero scale adc input    */
#define ADC_MAX             0x03FF      	/* full scale adc input    */

/* Default record strobe pulse length */
#define REC_PULSE_DURATION	20

/*** System Structures *****************************************************/

//#define CAPDATA_SIZE		250				/* 0.5 seconds of capture data */
// #define CAPDATA_SIZE		0				/* 0.5 seconds of capture data */

typedef struct _CAPDATA
{
	long dac_takeup;
	long dac_supply;
	long vel_takeup;
	long vel_supply;
	long rad_supply;
	long rad_takeup;
	long tape_tach;
	long tension;
} CAPDATA;

/* This structure contains runtime and program configuration data that is
 * stored and read from EEPROM. The structure size must be 4 byte aligned.
 */

typedef struct _SYSPARMS
{
    unsigned long magic;
    unsigned long version;

    /*** GLOBAL PARAMETERS ***/

    long debug;                     	/* debug level */
    long velocity_detect;           	/* stop detect threshold vel (10) 	 */
    long null_offset_gain;          	/* reel servo null offset gain 		 */
    long shuttle_slow_velocity;     	/* velocity to reduce sppeed to      */
    long shuttle_slow_offset;       	/* null offset to reduce velocity at */
    long engage_pinch_roller;			/* engage pinch roller during play   */
    long brakes_stop_play;				/* use brakes to stop from play mode */
    long pinch_settle_time;		   		/* delay before engaging play mode   */
    long lifter_settle_time;		  	/* tape lifer settling time in ms    */
    long record_pulse_length;			/* record pulse length time          */
    long reserved2;

    /*** STOP SERVO PARAMETERS ***/

    long stop_supply_tension;       	/* supply tension level (0-DAC_MAX)  */
    long stop_takeup_tension;       	/* takeup tension level (0-DAC_MAX)  */
    long stop_max_torque;           	/* must be <= DAC_MAX */
    long stop_min_torque;
    long stop_brake_torque;   			/* stop brake torque in shuttle mode */
    long reserved3;
    long reserved4;

    /*** SHUTTLE SERVO PARAMETERS ***/

    long shuttle_supply_tension;    	/* play supply tension level (0-DAC_MAX) */
    long shuttle_takeup_tension;    	/* play takeup tension level (0-DAC_MAX) */
    long shuttle_max_torque;        	/* must be <= DAC_MAX */
    long shuttle_min_torque;
    long shuttle_velocity;          	/* max shuttle speed (2000 - 10000)      */
    /* reel servo PID values */
    long reserved5;
    long shuttle_servo_pgain;       	/* P-gain */
    long shuttle_servo_igain;       	/* I-gain */
    long shuttle_servo_dgain;       	/* D-gain */
    /* tension sensor PID values */
    long reserved6;

    /*** PLAY SERVO PARAMETERS ***/

    long play_lo_supply_tension;		/* play supply tension level (0-DAC_MAX) */
    long play_lo_takeup_tension;     	/* play takeup tension level (0-DAC_MAX) */
    long play_hi_supply_tension;     	/* play supply tension level (0-DAC_MAX) */
    long play_hi_takeup_tension;     	/* play takeup tension level (0-DAC_MAX) */
    long play_max_torque;            	/* must be <= DAC_MAX */
    long play_min_torque;
    long play_tension_gain;				/* play tension velocity gain factor   ) */
    long play_hi_boost_start;
    long play_hi_boost_end;
    long play_lo_boost_start;
    long play_lo_boost_end;
    long play_lo_boost_time;			/* duration of play boost acceleration   */
    long play_lo_boost_step;		 	/* amount to decrement boost count by    */
    long play_hi_boost_time;
    long play_hi_boost_step;
    long reserved10;
} SYSPARMS;

/*** SERVO & PID LOOP DATA *************************************************/

/* Reel Torque Motor Servo Data */

typedef struct _SERVODATA
{
	long			mode;				/* the current servo mode        */
	long 			direction;			/* 1 = fwd or -1 = reverse       */
	long			velocity;		    /* sum of both reel velocities   */
	long			velocity_supply;	/* supply tach count per sample  */
	long 			velocity_takeup;    /* takeup tach count per sample  */
	unsigned long	tape_tach;			/* tape roller tachometer        */
	long			stop_null_supply;	/* stop mode supply null         */
	long			stop_null_takeup;	/* stop mode takeup null         */
	long			brake_torque;
	long			brake_state;
	long			offset_null;       	/* takeup/supply tach difference */
	long			offset_null_sum;
	long			offset_sample_cnt;
	long 			offset_takeup;		/* takeup null offset value      */
	long			offset_supply;		/* supply null offset value      */
	long			play_boost_count;
    long 			play_boost_time;	/* play boost timer counter      */
    long			play_boost_step;	/* decrement boost time step     */
    long			play_boost_start;
    long			play_boost_end;
    long			play_tension_gain;
    long 			play_supply_tension;
    long 			play_takeup_tension;
    long			tsense;				/* tension sensor value 		 */
    long 			tsense_sum;
	long			tsense_sample_cnt;
    uint32_t		adc[8];				/* ADC values (tension, etc)     */
	unsigned long	dac_takeup;			/* current takeup DAC level      */
	unsigned long	dac_supply;			/* current supply DAC level      */
	unsigned long 	dac_halt_supply;	/* halt mode DAC level           */
	unsigned long	dac_halt_takeup;	/* halt mode DAC level           */
	IPID 			pid;				/* servo loop PID data           */
	/*** Debug Variables ***/
	long 			db_cv;
	long 			db_error;
	long			db_debug;
} SERVODATA;

/*** Macros & Function Prototypes ******************************************/

/* Servo Mode Constants */

#define MODE_HALT       0       		/* all servo motion halted		*/
#define MODE_STOP       1       		/* servo stop mode				*/
#define MODE_PLAY       2       		/* servo play mode				*/
#define MODE_FWD        3       		/* servo forward mode			*/
#define MODE_REW        4       		/* servo rewind mode			*/

#define M_RECORD		0x080			/* upper bit indicats record    */

#define MODE_MASK		0x07

/* Macros to get/set servo mode */

#define SET_SERVO_MODE(m)		(g_cmode = (m & MODE_MASK))
#define GET_SERVO_MODE()		(g_cmode & MODE_MASK)
#define IS_SERVO_MODE(m)		(((g_cmode & MODE_MASK) == m) ? 1 : 0)
#define IS_STOPPED()         	((g_servo.velocity <= g_sys.velocity_detect) ? 1 : 0)

/* General Purpose Defines and Macros */

#define TAPE_DIR_FWD			(-1)	/* play, fwd direction */
#define TAPE_DIR_REW			(1)		/* rewind direction    */

#define DAC_CLAMP(dac, min, max)    \
{                                   \
    if (dac < min)                  \
        dac = min;                  \
    else if (dac > max)             \
        dac = max;                  \
}                                   \

/* main.c */
void MotorDAC_write(uint32_t supply, uint32_t takeup);
void InitSysDefaults(SYSPARMS* p);
int SysParamsWrite(SYSPARMS* sp);
int SysParamsRead(SYSPARMS* sp);

/* End-Of-File */
