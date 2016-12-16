/*
 * ServoTask.h
 *
 *  Created on: Jan 27, 2012
 *      Author: bstarr
 */

#ifndef DTC1200_MOTORDAC_H_
#define DTC1200_MOTORDAC_H_

void MotorDAC_initialize(void);
void MotorDAC_write(uint32_t supply, uint32_t takeup);

#endif
