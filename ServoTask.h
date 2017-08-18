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

#ifndef DTC1200_TIVATM4C123AE6PMI_SERVOTASK_H_
#define DTC1200_TIVATM4C123AE6PMI_SERVOTASK_H_

/* Tape moves at 30 IPS in high speed and 15 IPS in low speed. We want to
 * sample and correct at 0.3" intervals so our sample rate needs to
 * be 100 Hz (30/0.3 = 100). The timer delay period required in
 * milliseconds is 1/Hz * 1000. Therefore 1/100*1000 = 10ms. Therefore,
 * if we to correct at 0.15" intervals, the sample period would be 5ms.
 */

/* This macro is used to calculate the RPM of the motor based on velocity.
 * We are using Austria Micro Systems AS5047D with 500 pulses per revolution.
 * The QEI module is configured to capture both quadrature edges. Thus,
 * the 500 PPR encoder at four edges per line, gives us 2000 edges per rev.
 *
 * The period of the timer is configurable by specifying the load value
 * for the timer in the QEILOAD register. We can calculate RPM with
 * the following equation:
 *
 *	RPM = (clock * (2 ^ VelDiv) * Speed * 60) / (Load * PPR * Edges)
 *
 * For our case, consider a motor running at 600 rpm. A 500 pulse per
 * revolution quadrature encoder is attached to the motor, producing 2000
 * phase edges per revolution. With a velocity pre-divider of ÷1
 * (VelDiv set to 0) and clocking on both PhA and PhB edges, this results
 * in 20,000 pulses per second (the motor turns 10 times per second).
 * If the timer were clocked at 80,000,000 Hz, and the load value was
 * 20,000,000 (1/4 of a second), it would count 20000 pulses per update.
 *
 *	RPM = (50,000,000 * 1 * s * 60) / (500,000 * 360 * 4) = 600 rpm
 *	RPM = (100 * s) / 24 = 600 rpm
 *	RPM = (25 * s) / 6 = 600 rpm
 *
 *	RPM = (80,000,000 * 1 * s * 60) / (800,000 * 500 * 4) = 600 rpm
 *	RPM = (100 * s) / 33.33 = 600 rpm
 *
 */

#define QEI_BASE_SUPPLY	    QEI0_BASE   	/* QEI-0 is SUPPLY encoder */
#define QEI_BASE_TAKEUP	    QEI1_BASE   	/* QEI-1 is TAKEUP encoder */

#define QE_PPR				500				/* encoder pulses per revolution       */
#define QE_EDGES_PER_REV	(QE_PPR * 4)	/* PPR x 4 for four quad encoder edges */
#define QE_TIMER_PERIOD		800000			/* period of 800,000 is 10ms at 80MHz  */

/* Calculate RPM from the velocity value */
#define RPM(s)				((25 * s) / 6)

/*** Servo Mode Constants **************************************************/

#define MODE_HALT       	0       		/* all servo motion halted		*/
#define MODE_STOP       	1       		/* servo stop mode				*/
#define MODE_PLAY       	2       		/* servo play mode				*/
#define MODE_FWD        	3       		/* servo forward mode			*/
#define MODE_REW        	4       		/* servo rewind mode			*/

#define M_RECORD			0x080			/* upper bit indicates record   */

#define MODE_MASK			0x07

/* General Purpose Defines and Macros */

#define TAPE_DIR_FWD		(-1)			/* play, fwd direction */
#define TAPE_DIR_REW		(1)				/* rewind direction    */

/*** Function Prototypes ***************************************************/

/* Servo Mode Functions */
void ServoSetMode(uint32_t mode);
uint32_t ServoGetMode(void);
int32_t IsServoMode(uint32_t mode);
int32_t IsServoMotion(void);

#define SET_SERVO_MODE(m)		ServoSetMode(m)
#define GET_SERVO_MODE()		ServoGetMode()
#define IS_SERVO_MODE(m)		IsServoMode(m)
#define IS_SERVO_MOTION()		IsServoMotion()

Void ServoLoopTask(UArg a0, UArg a1);

#endif /* DTC1200_TIVATM4C123AE6PMI_SERVOTASK_H_ */
