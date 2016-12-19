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

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_sysctl.h"
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_ints.h>
#include <inc/hw_gpio.h>
#include "inc/hw_ssi.h"
#include "inc/hw_i2c.h"

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

#include <driverlib/gpio.h>
#include <driverlib/flash.h>
#include <driverlib/eeprom.h>
#include <driverlib/sysctl.h>
#include <driverlib/interrupt.h>
#include <driverlib/timer.h>
#include <driverlib/i2c.h>
#include <driverlib/ssi.h>
#include <driverlib/udma.h>
#include <driverlib/adc.h>
#include <driverlib/qei.h>
#include <driverlib/pin_map.h>

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Gate.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* Project specific includes */
#include "DTC1200.h"
#include "TachTimer.h"

/****************************************************************************
 * Static Data Items
 ****************************************************************************/

#define TACH_EDGE_COUNT		(11)		//0x03E8

static uint32_t Timer_A_Count = 0;
static uint32_t Timer_B_Count = 0;

/* Hardware Interrupt Handlers */
static Void Timer2AIntHandler(void);
static Void Timer2BIntHandler(void);

/****************************************************************************
 * The transport has tape tach derived from the search-to-cue timer card
 * using the quadrature encoder from the tape timer roller. The pulse stream
 * is approximately 240 Hz with tape moving at 30 IPS. We configure
 * TIMER2A as 16-bit input edge count mode as described in the data sheet.
 *
 * In Input Edge Count Mode, the timer stops after the desired number of
 * edge events has been detected. To re-enable the timer, ensure that the
 * TnEN bit is cleared and repeat step 4 through step 9.
 ****************************************************************************/

void Tachometer_initialize(void)
{
	Timer_A_Count = 0;
	Timer_B_Count = 0;

	/* Map the timer interrupt handlers. We don't make sys/bios calls
	 * from these interrupt handlers and there is no need to create a
	 * context handler with stack swapping for these. These handlers
	 * just update some globals variables and need to execute as
	 * quickly and efficiently as possible.
	 */
	Hwi_plug(INT_TIMER2A_TM4C123, Timer2AIntHandler);
	Hwi_plug(INT_TIMER2B_TM4C123, Timer2BIntHandler);

	/* Disable interrupts */
	IntMasterDisable();

	/* Enable Timers */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    /* Enable pin PB0 for TIMER2 T2CCP0 */
    GPIOPinConfigure(GPIO_PB0_T2CCP0);
    GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_0);

    /* Enable pin PB1 for TIMER2 T2CCP1 */
    GPIOPinConfigure(GPIO_PB1_T2CCP1);
    GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_1);

	/* Configure Edge Count Timers */

	/* Setup timer2 as two 16-bit timers in edge count mode. The counters
	 * will count TACH_EDGE_COUNT edges then generate an interrupt.
	 */
	TimerConfigure(TIMER2_BASE, (TIMER_CFG_SPLIT_PAIR |
								 TIMER_CFG_A_CAP_COUNT |
								 TIMER_CFG_B_CAP_COUNT));

	/* The prescaler not available for Edge Count mode, set to 0 for good measure */
	TimerPrescaleSet(TIMER2_BASE, TIMER_BOTH, 0);
	/* Trigger on rising edges */
	TimerControlEvent(TIMER2_BASE, TIMER_BOTH, TIMER_EVENT_POS_EDGE);
	/* Load set timer value */
	TimerLoadSet(TIMER2_BASE, TIMER_BOTH, TACH_EDGE_COUNT);
	/* Load the match value */
	TimerMatchSet(TIMER2_BASE, TIMER_BOTH, 0x0000);

	/* Enable interrupts */
	IntMasterEnable();

	IntEnable(INT_TIMER2A);
	IntEnable(INT_TIMER2B);

	TimerIntEnable(TIMER2_BASE, TIMER_CAPA_MATCH);
	TimerIntEnable(TIMER2_BASE, TIMER_CAPB_MATCH);

	/* Configure time base timer (free running) */

	/* Setup timer as 32-bit periodic timer */
	TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
	/* Load timer value (TIMER_A for 32-bit operation) */
	TimerLoadSet(TIMER1_BASE, TIMER_A, 0xFFFFFFFF);

	/* Enable timers */
	TimerEnable(TIMER2_BASE, TIMER_BOTH);
	TimerEnable(TIMER1_BASE, TIMER_A);
}

/****************************************************************************
 * INTERRUPT HANDLER : Compares timer count from previous interrupt and
 * stores the difference in a global variable
 ****************************************************************************/

Void Timer2AIntHandler(void)
{
	static uint32_t previous_time = 0;

	TimerIntClear(TIMER2_BASE, TIMER_CAPA_MATCH);
	TimerLoadSet(TIMER2_BASE, TIMER_A, TACH_EDGE_COUNT);
	TimerEnable(TIMER2_BASE, TIMER_A);

	Timer_A_Count = previous_time - TimerValueGet(TIMER1_BASE, TIMER_A);
	previous_time = TimerValueGet(TIMER1_BASE, TIMER_A);
}

/****************************************************************************
 * INTERRUPT HANDLER : Compares timer count from previous interrupt and
 * stores the difference in a global variable
 ****************************************************************************/

Void Timer2BIntHandler(void)
{
	//static uint32_t previous_time = 0;

	TimerIntClear(TIMER2_BASE, TIMER_CAPB_MATCH);
	TimerLoadSet(TIMER2_BASE, TIMER_B, TACH_EDGE_COUNT);
	TimerEnable(TIMER2_BASE, TIMER_B);
#if 0
	Timer_B_Count = previous_time - TimerValueGet(TIMER1_BASE, TIMER_A);
	previous_time = TimerValueGet(TIMER1_BASE, TIMER_A);
#endif
}

/****************************************************************************
 * Read the current tape tachometer count
 *	Clock running at 80MHz, counter is number of clock cycles
 *	in TACH_EDGE_COUNT (1000) rising edges
 *	total period = count/80000000 s
 *	single period = count/(80000000 * 1000)
 *	frequency = (80000000 * 1000)/count
 *	to prevent overflow: (800000000/count) * 10
 ****************************************************************************/

uint32_t ReadTapeTach(void)
{
	uint32_t a;

	IArg key = Gate_enterModule();  // enter critical section

	if (Timer_A_Count == 0)
		a = 0;
	else
		a = (800000000 / Timer_A_Count);

	Gate_leaveModule(key);

	return a;
}

void ResetTapeTach(void)
{
	IArg key = Gate_enterModule();  // enter critical section
	Timer_A_Count = 0;
	Gate_leaveModule(key);
}

/****************************************************************************
 * Read the current capstan motor tachometer count
 ****************************************************************************/

uint32_t ReadCapstanTach(void)
{
	uint32_t b;

	IArg key = Gate_enterModule();  // enter critical section

	if (Timer_B_Count == 0)
		b = 0;
	else
		b = (800000000 / Timer_B_Count);

	Gate_leaveModule(key);

	return b;
}

void ResetCapstanTach(void)
{
	IArg key = Gate_enterModule();  // enter critical section
	Timer_B_Count = 0;
	Gate_leaveModule(key);
}

/* End-Of-File */
