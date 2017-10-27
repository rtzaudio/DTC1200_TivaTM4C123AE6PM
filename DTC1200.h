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
#define FIRMWARE_REV        9        	/* firmware revision */

#define MAGIC               0xCEB0FACE  /* magic number for EEPROM data */
#define MAKEREV(v, r)       ((v << 16) | (r & 0xFFFF))

#define UNDEFINED           ((uint32_t)(-1))

/* Timeout for SPI communications */
#define TIMEOUT_SPI			500

/* Default record strobe pulse length */
#define REC_PULSE_TIME		50
#define REC_SETTLE_TIME		10

/*** Build/Config Options **************************************************/

#define DEBUG_LEVEL			0
#define BUTTON_INTERRUPTS	0			/* 1=interrupt, 0=polled buttons */
#define CAPDATA_SIZE		0			/* 250 = 0.5 sec of capture data */

/*** System Structures *****************************************************/

#if (CAPDATA_SIZE > 0)
typedef struct _CAPDATA
{
	uint32_t dac_takeup;
	uint32_t dac_supply;
	int32_t vel_takeup;
	int32_t vel_supply;
	int32_t rad_supply;
	int32_t rad_takeup;
	int32_t tape_tach;
	int32_t tension;
} CAPDATA;
#endif

/* This structure contains runtime and program configuration data that is
 * stored and read from EEPROM. The structure size must be 4 byte aligned.
 */

typedef struct _SYSPARMS
{
	uint32_t magic;
	uint32_t version;

    /*** GLOBAL PARAMETERS ***/

    int32_t debug;                     	/* debug level */
    int32_t vel_detect_threshold;       /* vel detect threshold (10) 	     */
    int32_t null_offset_gain;          	/* reel servo null offset gain 		 */
    int32_t shuttle_slow_velocity;     	/* velocity to reduce speed to       */
    int32_t shuttle_slow_offset;       	/* null offset to reduce velocity at */
    int32_t pinch_settle_time;		   	/* delay before engaging play mode   */
    int32_t lifter_settle_time;	  	    /* lifter settling time in ms        */
    int32_t brake_settle_time;  		/* break settling time after STOP    */
    int32_t play_settle_time;			/* play after shuttle settling time  */
    int32_t rechold_settle_time;		/* record pulse length time          */
    int32_t record_pulse_time;			/* record pulse length time          */
    int32_t tension_sensor_gain;		/* tension sensor gain divisor       */
    uint32_t debounce;					/* debounce transport buttons time   */
    uint32_t sysflags;					/* global system bit flags           */

    /*** STOP SERVO PARAMETERS ***/

    int32_t stop_supply_tension;       	/* supply tension level (0-DAC_MAX)  */
    int32_t stop_takeup_tension;       	/* takeup tension level (0-DAC_MAX)  */
    int32_t stop_max_torque;           	/* must be <= DAC_MAX */
    int32_t stop_min_torque;
    int32_t stop_brake_torque;   		/* stop brake torque in shuttle mode */
    int32_t reserved3;
    int32_t reserved4;

    /*** SHUTTLE SERVO PARAMETERS ***/

    int32_t shuttle_supply_tension;    	/* play supply tension level (0-DAC_MAX) */
    int32_t shuttle_takeup_tension;    	/* play takeup tension level (0-DAC_MAX) */
    int32_t shuttle_max_torque;        	/* must be <= DAC_MAX */
    int32_t shuttle_min_torque;
    int32_t shuttle_velocity;          	/* max shuttle speed (2000 - 10000)      */
    /* reel servo PID values */
    int32_t reserved5;
    int32_t shuttle_servo_pgain;       	/* P-gain */
    int32_t shuttle_servo_igain;       	/* I-gain */
    int32_t shuttle_servo_dgain;       	/* D-gain */
    /* tension sensor PID values */
    int32_t reserved6;

    /*** PLAY SERVO PARAMETERS ***/

    int32_t play_lo_supply_tension;		/* play supply tension level (0-DAC_MAX) */
    int32_t play_lo_takeup_tension;    	/* play takeup tension level (0-DAC_MAX) */
    int32_t play_hi_supply_tension;    	/* play supply tension level (0-DAC_MAX) */
    int32_t play_hi_takeup_tension;    	/* play takeup tension level (0-DAC_MAX) */
    int32_t play_max_torque;           	/* must be <= DAC_MAX */
    int32_t play_min_torque;
    int32_t play_tension_gain;			/* play tension velocity gain factor   ) */
    int32_t play_hi_boost_start;
    int32_t play_hi_boost_end;
    int32_t play_lo_boost_start;
    int32_t play_lo_boost_end;
    int32_t play_lo_boost_time;			/* duration of play boost acceleration   */
    int32_t play_lo_boost_step;		 	/* amount to decrement boost count by    */
    int32_t play_hi_boost_time;
    int32_t play_hi_boost_step;
    int32_t reserved10;
} SYSPARMS;

/* System Bit Flags for SYSPARAMS.sysflags */

#define SF_LIFTER_AT_STOP			0x0001	/* leave lifter engaged at stop */
#define SF_BRAKES_AT_STOP			0x0002	/* leave brakes engaged at stop */
#define SF_BRAKES_STOP_PLAY			0x0004	/* use brakes to stop play mode */
#define SF_ENGAGE_PINCH_ROLLER		0x0008	/* engage pinch roller at play  */

/*** SERVO & PID LOOP DATA *************************************************/

/* Reel Torque Motor Servo Data */

typedef struct _SERVODATA
{
	uint32_t	mode;				/* the current servo mode        */
	uint32_t	mode_prev;			/* previous servo mode           */
	int32_t		motion;				/* servo motion flag             */
	int32_t 	direction;			/* 1 = fwd or -1 = reverse       */
	int32_t		velocity;		    /* sum of both reel velocities   */
	int32_t		velocity_supply;	/* supply tach count per sample  */
	int32_t 	velocity_takeup;    /* takeup tach count per sample  */
	uint32_t	tape_tach;			/* tape roller tachometer        */
	int32_t		stop_torque_supply;	/* stop mode supply null         */
	int32_t		stop_torque_takeup;	/* stop mode takeup null         */
	int32_t		stop_brake_state;	/* stop servo dynamic brake state*/
	int32_t		offset_null;       	/* takeup/supply tach difference */
	int32_t		offset_null_sum;
	int32_t		offset_sample_cnt;
	int32_t 	offset_takeup;		/* takeup null offset value      */
	int32_t		offset_supply;		/* supply null offset value      */
	int32_t		play_boost_count;
    int32_t 	play_boost_time;	/* play boost timer counter      */
    int32_t		play_boost_step;	/* decrement boost time step     */
    int32_t		play_boost_start;
    int32_t		play_boost_end;
    int32_t		play_tension_gain;
    int32_t 	play_supply_tension;
    int32_t 	play_takeup_tension;
	int32_t		rpm_takeup;
	int32_t		rpm_takeup_sum;
	int32_t		rpm_supply;
	int32_t		rpm_supply_sum;
	int32_t		rpm_sum_cnt;
	uint32_t	qei_takeup_error_cnt;
	uint32_t	qei_supply_error_cnt;
    int32_t		tsense;				/* tension sensor value 		 */
    int32_t 	tsense_sum;
	int32_t		tsense_sample_cnt;
    uint32_t	adc[8];				/* ADC values (tension, etc)     */
    uint32_t	dac_takeup;			/* current takeup DAC level      */
    uint32_t	dac_supply;			/* current supply DAC level      */
    uint32_t 	dac_halt_supply;	/* halt mode DAC level           */
    uint32_t	dac_halt_takeup;	/* halt mode DAC level           */
	IPID 		pid;				/* servo loop PID data           */
	/*** Debug Variables ***/
	int32_t 	db_cv;
	int32_t 	db_error;
	int32_t		db_debug;
} SERVODATA;

/*** Macros & Function Prototypes ******************************************/

/* main.c */
void InitSysDefaults(SYSPARMS* p);
int32_t SysParamsWrite(SYSPARMS* sp);
int32_t SysParamsRead(SYSPARMS* sp);

/* End-Of-File */
