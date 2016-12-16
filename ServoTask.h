/*
 * ServoTask.h
 *
 *  Created on: Jan 27, 2012
 *      Author: bstarr
 */

#ifndef DTC1200_TIVATM4C123AE6PMI_SERVOTASK_H_
#define DTC1200_TIVATM4C123AE6PMI_SERVOTASK_H_

void SetServoMode(long mode);
long GetServoMode(void);

Void ServoLoopTask(UArg a0, UArg a1);

void MotorDAC_write(uint32_t supply, uint32_t takeup);

#endif /* DTC1200_TIVATM4C123AE6PMI_SERVOTASK_H_ */
