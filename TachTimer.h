/*
 * ServoTask.h
 *
 *  Created on: Jan 27, 2012
 *      Author: bstarr
 */

#ifndef DTC1200_TACH_TIMER_H_
#define DTC1200_TACH_TIMER_H_

void Tachometer_initialize(void);

uint32_t ReadTapeTach(void);
void ResetTapeTach(void);

uint32_t ReadCapstanTach(void);
void ResetCapstanTach(void);

#endif
