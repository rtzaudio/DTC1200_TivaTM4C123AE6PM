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
#define FIRMWARE_REV        32        	/* firmware revision */
#define FIRMWARE_BUILD      1

#define FIRMWARE_MIN_BUILD  3

#define MAGIC               0xCEB0FACE  /* magic number for EEPROM data */
#define MAKEREV(v, r)       ((v << 16) | (r & 0xFFFF))

#define UNDEFINED           ((uint32_t)(-1))

/* Timeout for SPI communications */
#define TIMEOUT_SPI			500

/* Default record strobe pulse length */
#define REC_PULSE_TIME		50
#define REC_SETTLE_TIME		10

/*** ADC Specific **********************************************************/

#define VREF                3.3f

#define ADC_TO_CELCIUS(c)           ( 147.5f - ((75.0f * VREF * (float)c) / 4096.0f) )
#define CELCIUS_TO_FAHRENHEIT(c)    ( (float)c * 1.8f + 32.0f )

/*** Build/Config Options **************************************************/

#define DEBUG_LEVEL			0
#define BUTTON_INTERRUPTS	0			/* 1=interrupt, 0=polled buttons */
#define CAPDATA_SIZE		0			/* 250 = 0.5 sec of capture data */

/*** System Structures *****************************************************/

#if (CAPDATA_SIZE > 0)
typedef struct _CAPDATA
{
	float dac_takeup;
	float dac_supply;
	float vel_takeup;
	float vel_supply;
	float rad_supply;
	float rad_takeup;
	float tape_tach;
	float tension;
} CAPDATA;
#endif

/* This structure contains runtime and program configuration data that is
 * stored and read from EEPROM. The structure size must be 4 byte aligned.
 */

typedef struct _SYSPARMS
{
	uint32_t magic;
	uint32_t version;
	uint32_t build;

    /*** GLOBAL PARAMETERS ***/

    int32_t debug;                     	/* debug level */
    int32_t pinch_settle_time;		   	/* delay before engaging play mode   */
    int32_t lifter_settle_time;	  	    /* lifter settling time in ms        */
    int32_t brake_settle_time;  		/* break settling time after STOP    */
    int32_t play_settle_time;			/* play after shuttle settling time  */
    int32_t rechold_settle_time;		/* record pulse length time          */
    int32_t record_pulse_time;			/* record pulse length time          */
    int32_t vel_detect_threshold;       /* vel detect threshold (10)         */
    uint32_t debounce;					/* debounce transport buttons time   */
    uint32_t sysflags;					/* global system bit flags           */

    /*** SOFTWARE GAIN PARAMETERS ***/

    float   reel_radius_gain;           /* reeling radius play gain factor   */
    float   reel_offset_gain;           /* reeling radius offset gain factor */
    float   tension_sensor_gain;        /* tension sensor gain divisor       */

    /*** STOP SERVO PARAMETERS ***/

    int32_t stop_supply_tension;       	/* supply tension level (0-DAC_MAX)  */
    int32_t stop_takeup_tension;       	/* takeup tension level (0-DAC_MAX)  */
    int32_t stop_brake_torque;   		/* stop brake torque in shuttle mode */

    /*** SHUTTLE SERVO PARAMETERS ***/

    int32_t shuttle_supply_tension;    	/* play supply tension (0-DAC_MAX)   */
    int32_t shuttle_takeup_tension;    	/* play takeup tension               */
    int32_t shuttle_velocity;          	/* target speed for shuttle mode     */
    int32_t shuttle_lib_velocity;       /* library wind mode velocity        */
    int32_t shuttle_autoslow_velocity;  /* velocity to reduce speed to       */
    int32_t autoslow_at_offset;         /* auto-slow trigger at offset       */
    int32_t autoslow_at_velocity;       /* auto-slow trigger at velocity     */
    float   shuttle_fwd_holdback_gain;  /* velocity tension gain factor      */
    float   shuttle_rew_holdback_gain;  /* velocity tension gain factor      */
    /* reel servo PID values */
    float   shuttle_servo_pgain;       	/* P-gain */
    float   shuttle_servo_igain;       	/* I-gain */
    float   shuttle_servo_dgain;       	/* D-gain */

    /*** PLAY SERVO PARAMETERS ***/

    /* play high speed boost parameters */
    int32_t play_hi_supply_tension;    	/* play supply tension level (0-DAC_MAX) */
    int32_t play_hi_takeup_tension;    	/* play takeup tension level (0-DAC_MAX) */
    int32_t play_hi_boost_end;
    float   play_hi_boost_pgain;       	/* P-gain */
    float   play_hi_boost_igain;       	/* I-gain */
    /* play low speed boost parameters */
    int32_t play_lo_supply_tension;		/* play supply tension level (0-DAC_MAX) */
    int32_t play_lo_takeup_tension;    	/* play takeup tension level (0-DAC_MAX) */
    int32_t play_lo_boost_end;
    float   play_lo_boost_pgain;   		/* P-gain */
    float   play_lo_boost_igain;   		/* I-gain */
} SYSPARMS;

/* System Bit Flags for SYSPARAMS.sysflags */

#define SF_LIFTER_AT_STOP			0x0001	/* leave lifter engaged at stop */
#define SF_BRAKES_AT_STOP			0x0002	/* leave brakes engaged at stop */
#define SF_BRAKES_STOP_PLAY			0x0004	/* use brakes to stop play mode */
#define SF_ENGAGE_PINCH_ROLLER		0x0008	/* engage pinch roller at play  */
#define SF_STOP_AT_TAPE_END         0x0010  /* stop @tape end leader detect */

/*** SERVO & PID LOOP DATA *************************************************/

/* Reel Torque Motor Servo Data */

typedef struct _SERVODATA
{
	uint32_t	mode;					/* the current servo mode        */
	uint32_t	mode_prev;				/* previous servo mode           */
	int32_t		motion;					/* servo motion flag             */
	int32_t 	direction;				/* 1 = fwd or -1 = reverse       */
	int32_t     direction_state;        /* last shuttle direction state  */
    float       holdback;               /* back tension during shuttle   */
	float		velocity;		    	/* sum of both reel velocities   */
	float		velocity_supply;		/* supply tach count per sample  */
	float 		velocity_takeup;    	/* takeup tach count per sample  */
	float		tape_tach;				/* tape roller tachometer        */
	float		radius_takeup;			/* takeup reel reeling radius    */
    float       radius_takeup_accum;    /* takeup radius accumulator     */
	float		radius_supply;			/* supply reel reeling radius    */
    float       radius_supply_accum;    /* supply radius accumulator     */
	float		stop_torque_supply;		/* stop mode supply null         */
	float		stop_torque_takeup;		/* stop mode takeup null         */
	int32_t		stop_brake_state;		/* stop servo dynamic brake state*/
	uint32_t	offset_sample_cnt;
	float		offset_null;       		/* takeup/supply tach difference */
	float		offset_null_accum;
	float 		offset_takeup;			/* takeup null offset value      */
	float		offset_supply;			/* supply null offset value      */
	uint32_t	play_boost_count;
    int32_t		play_boost_end;			/* tape velocity to exit boost   */
    float		play_supply_tension;
    float		play_takeup_tension;
    uint32_t    shuttle_velocity;
	uint32_t	qei_takeup_error_cnt;
	uint32_t	qei_supply_error_cnt;
    uint32_t	adc[8];					/* ADC values (tension, etc)     */
    float		tsense;					/* tension sensor value 		 */
    float		cpu_temp;				/* CPU temp included in ADC read */
    float		dac_takeup;				/* current takeup DAC level      */
    float		dac_supply;				/* current supply DAC level      */
    uint32_t 	dac_halt_supply;		/* halt mode DAC level           */
    uint32_t	dac_halt_takeup;		/* halt mode DAC level           */
	FPID		pid_shuttle;			/* PID for shuttle velocity ctrl */
	FPID		pid_play;				/* PID for play mode boost stage */
	/*** Debug Variables ***/
	float 		db_cv;
	float 		db_error;
	float		db_debug;
} SERVODATA;

/*** Macros & Function Prototypes ******************************************/

/* main.c */
void InitSysDefaults(SYSPARMS* p);
int32_t SysParamsWrite(SYSPARMS* sp);
int32_t SysParamsRead(SYSPARMS* sp);

/* End-Of-File */
