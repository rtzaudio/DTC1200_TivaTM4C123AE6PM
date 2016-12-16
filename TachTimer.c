/*****************************************************************************
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2012, RTZ Professional Audio
 *
 * The source code contained in this file and associated files is copyright
 * protected material. This material contains the confidential, proprietary
 * and trade secret information of Sigma Software Research. No disclosure or
 * use of any portions of this material may be made without the express
 * written consent of RTZ Professional Audio.
 *****************************************************************************/

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

static uint32_t s_sysClockSpeed;

static uint32_t Timer_A_Count = 0;
static uint32_t Timer_B_Count = 0;

/* Hardware Interrupt Handlers */
static Void TimerAIntHandler(void);
static Void TimerBIntHandler(void);

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
	uint32_t s_sysClockSpeed = SysCtlClockGet();

	// Disable interrupts
	IntMasterDisable();

	// Map the timer interrupt handlers. We don't make sys/bios calls
	// from these interrupt handlers and there is no need to create a
	// context handler with stack swapping for these. These handlers
	// just update some globals variables and need to execute as
	// quickly and efficiently as possible.
	Hwi_plug(INT_TIMER2A_TM4C123, TimerAIntHandler);
	Hwi_plug(INT_TIMER2B_TM4C123, TimerBIntHandler);

	// Enable Timers
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);		
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);    
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);

    // Enable pin PB0 for TIMER2 T2CCP0
    GPIOPinConfigure(GPIO_PB0_T2CCP0);
    GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_0);

    // Enable pin PB1 for TIMER2 T2CCP1
    GPIOPinConfigure(GPIO_PB1_T2CCP1);
    GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_1);

    // Reset the timers
	SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER1);
	SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER2);

	// Configure Edge Count Timers

	// Setup timer as 2 16-bit timers in Edge Count mode
	// Counters will count 1000 edges then generate an interrupt
	TimerConfigure(TIMER2_BASE, (TIMER_CFG_SPLIT_PAIR |
								 TIMER_CFG_A_CAP_COUNT |
								 TIMER_CFG_B_CAP_COUNT));

	// The prescaler is not available for Edge Count mode
	// Set to 0 for good measure
	TimerPrescaleSet(TIMER2_BASE, TIMER_BOTH, 0);
	// Trigger on rising edges
	TimerControlEvent(TIMER2_BASE, TIMER_BOTH, TIMER_EVENT_POS_EDGE);
	// Load timer value
	TimerLoadSet(TIMER2_BASE, TIMER_BOTH, TACH_EDGE_COUNT);
	// Set match value
	TimerMatchSet(TIMER2_BASE, TIMER_BOTH, 0x0000);

	// Enable interrupts
	IntMasterEnable();

	TimerIntEnable(TIMER2_BASE, TIMER_CAPA_MATCH);
	TimerIntEnable(TIMER2_BASE, TIMER_CAPB_MATCH);

	// Configure time base timer (free running)

	// Setup timer as 32-bit periodic timer
	TimerConfigure(TIMER1_BASE, TIMER_CFG_A_PERIODIC_UP);
	// Load timer value (TIMER_A for 32-bit operation)
	TimerLoadSet(TIMER1_BASE, TIMER_A, 0xFFFFFFFF);

	// Enable timers
	TimerEnable(TIMER2_BASE, TIMER_BOTH);
	TimerEnable(TIMER1_BASE, TIMER_A);
}

/****************************************************************************
 * INTERRUPT HANDLER : Compares timer count from previous interrupt and
 * stores the difference in a global variable
 ****************************************************************************/

Void TimerAIntHandler(void)
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

Void TimerBIntHandler(void)
{
	static uint32_t previous_time = 0;

	TimerIntClear(TIMER2_BASE, TIMER_CAPB_MATCH);
	TimerLoadSet(TIMER2_BASE, TIMER_B, TACH_EDGE_COUNT);
	TimerEnable(TIMER2_BASE, TIMER_B);

	Timer_B_Count = previous_time - TimerValueGet(TIMER1_BASE, TIMER_A);

	previous_time = TimerValueGet(TIMER1_BASE, TIMER_A);
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
		a = (s_sysClockSpeed / Timer_A_Count);

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
		b = (s_sysClockSpeed / Timer_B_Count);

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
