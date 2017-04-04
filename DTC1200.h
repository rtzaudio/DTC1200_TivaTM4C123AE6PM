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
#define FIRMWARE_REV        1        	/* firmware revision */

#define MAGIC               0xCEB0FACE  /* magic number for EEPROM data */
#define MAKEREV(v, r)       ((v << 16) | (r & 0xFFFF))

#define UNDEFINED           (-1)

/* Timeout for SPI communications */
#define TIMEOUT_SPI			1000

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
    long tension_sensor_gain;			/* tension sensor gain divisor       */

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
	long			stop_brake_state;	/* stop servo dynamic brake state*/
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
