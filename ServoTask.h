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

#ifndef _SERVOTASK_H_
#define _SERVOTASK_H_

/*** Servo Mode Constants **************************************************/

#define MODE_HALT       	0       		/* all servo motion halted		*/
#define MODE_STOP       	1       		/* servo stop mode				*/
#define MODE_PLAY       	2       		/* servo play mode				*/
#define MODE_FWD        	3       		/* servo forward mode			*/
#define MODE_REW        	4       		/* servo rewind mode			*/

#define M_NOSLOW            0x20            /* no auto slow in shuttle mode */
#define M_LIBWIND			0x40			/* shuttle library wind flag    */
#define M_RECORD			0x80			/* upper bit indicates record   */

#define MODE_MASK			0x07

/* General Purpose Defines and Macros */

#define TAPE_DIR_FWD		(-1)			/* play, fwd direction */
#define TAPE_DIR_REW		(1)				/* rewind direction    */
#define TAPE_DIR_STOP		(0)

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

#endif /* _SERVOTASK_H_ */
