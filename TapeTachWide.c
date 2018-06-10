/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * Timer Logic contributed by Bruno Saraiva
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
#include <inc/hw_sysctl.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_ints.h>
#include <inc/hw_gpio.h>
#include <inc/hw_timer.h>
#include <inc/hw_ssi.h>
#include <inc/hw_i2c.h>

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
#include "TapeTach.h"


/****************************************************************************
 * Static Data Items
 ****************************************************************************/

static uint32_t g_systemClock = 1;

/* Hardware Interrupt Handlers */
static Void WTimer1AIntHandler(void);
static Void WTimer1BIntHandler(void);

#if (TACH_TYPE_EDGE_WIDTH > 0)

static TACHDATA g_tach;

/****************************************************************************
 * The transport has tape tach derived from the search-to-cue timer card
 * using the quadrature encoder from the tape timer roller. The pulse stream
 * is approximately 240 Hz with tape moving at 30 IPS. We configure
 * WTIMER1A as 32-bit input edge count mode.
  ****************************************************************************/

void TapeTach_initialize(void)
{
    g_systemClock = SysCtlClockGet();

	/* Map the timer interrupt handlers. We don't make sys/bios calls
	 * from these interrupt handlers and there is no need to create a
	 * context handler with stack swapping for these. These handlers
	 * just update some globals variables and need to execute as
	 * quickly and efficiently as possible.
	 */
	Hwi_plug(INT_WTIMER1A, WTimer1AIntHandler);
    Hwi_plug(INT_WTIMER1B, WTimer1BIntHandler);

    /* Enable the wide timer peripheral */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER1);
    while(!(SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOC))){}

    /* First make sure the timer is disabled */
    TimerDisable(WTIMER1_BASE, TIMER_A);
    TimerDisable(WTIMER1_BASE, TIMER_B);

    /* Disable global interrupts */
    IntMasterDisable();

    /* Enable pin PC6 for WTIMER1 WT1CCP0 */
    GPIOPinConfigure(GPIO_PC6_WT1CCP0);
    GPIOPinTypeTimer(GPIO_PORTC_BASE, GPIO_PIN_6);

	/* Configure Edge Count Timers */

    /* Configure wide timer for time capture, split mode. Timer-B is
     * used as a timeout timer that triggers when no edges are
     * present after half second timeout period.
     */
    TimerConfigure(WTIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME | TIMER_CFG_B_PERIODIC);

    /* Define event which generates interrupt on timer A */
    TimerControlEvent(WTIMER1_BASE, TIMER_A, TIMER_EVENT_NEG_EDGE);

    /* Configure the timeout count on timer B for half a second */
    TimerLoadSet(WTIMER1_BASE, TIMER_B, g_systemClock/2);

    /* Enable interrupt on timer A for capture event and timer B for timeout */
    TimerIntEnable(WTIMER1_BASE, TIMER_CAPA_EVENT | TIMER_TIMB_TIMEOUT);

    /* Enable timer A & B interrupts */
    IntEnable(INT_WTIMER1A);
    IntEnable(INT_WTIMER1B);

    /* Enable master interrupts */
    IntMasterEnable();

    /* Start timers A and B*/
    TimerEnable(WTIMER1_BASE, TIMER_BOTH);
}

/****************************************************************************
 * WTIMER1A FALLING EDGE CAPTURE TIMER INTERRUPT HANDLER
 ****************************************************************************/

Void WTimer1AIntHandler(void)
{
    uint32_t thisCount;
    uint32_t thisPeriod;
    uint32_t key;

    TimerIntClear(WTIMER1_BASE, TIMER_CAPA_EVENT);

    /* ENTER - Critical Section */
    key = Hwi_disable();

    thisCount = TimerValueGet(WTIMER1_BASE, TIMER_A);
    thisPeriod = g_tach.previousCount - thisCount;
    g_tach.previousCount = thisCount;

    if (thisPeriod)       /* Shield from dividing by zero */
    {
        /* Calculates and store averaged value */
        g_tach.averageSum -= g_tach.averageCount[g_tach.averageIdx];
        g_tach.averageSum += thisPeriod;
        g_tach.averageCount[g_tach.averageIdx] = thisPeriod;
        g_tach.averageIdx++;
        g_tach.averageIdx %= TACH_AVG_QTY;

        /* Sets the status to indicate tach is alive */
        g_tach.tachAlive = true;

        /* Store RAW value, which refers to one measurement only */
        g_tach.frequencyRawHz = (float)g_systemClock / (float)thisPeriod;

        /* Update the average sum */
        if (g_tach.averageSum)
            g_tach.frequencyAvgHz = (float)g_systemClock / ((float)g_tach.averageSum / (float)TACH_AVG_QTY);

        /* Resets timeout timer */
        HWREG(WTIMER1_BASE + TIMER_O_TBV) = g_systemClock / 2;
    }

    /* EXIT - Critical Section */
    Hwi_restore(key);
}

/****************************************************************************
 * WTIMER1B 1/2 SECOND EDGE DETECT TIMEOUT TIMER INTERRUPT HANDLER
 ****************************************************************************/

Void WTimer1BIntHandler(void)
{
    uint32_t key;

    TimerIntClear(WTIMER1_BASE, TIMER_TIMB_TIMEOUT);

    key = Hwi_disable();
    {
        g_tach.tachAlive      = false;
        g_tach.frequencyAvgHz = 0.0f;
        g_tach.frequencyRawHz = 0.0f;
    }
    Hwi_restore(key);
}

/****************************************************************************
 * Read the current tape tachometer count.
 ****************************************************************************/

float TapeTach_read(void)
{
    uint32_t key;
	float avg;

	key = Hwi_disable();
	{
	    avg = g_tach.frequencyAvgHz;
	}
	Hwi_restore(key);

	return avg;
}

/****************************************************************************
 * Reset the tach data.
 ****************************************************************************/

void TapeTach_reset(void)
{
    size_t i;
    uint32_t key;

    key = Hwi_disable();
    {
        for(i=0; i < TACH_AVG_QTY; i++)
            g_tach.averageCount[i] = 0;

        g_tach.previousCount  = 0;
        g_tach.averageSum     = 0;
        g_tach.averageIdx     = 0;

        g_tach.tachAlive      = false;
        g_tach.frequencyAvgHz = 0.0f;
        g_tach.frequencyRawHz = 0.0f;
    }
    Hwi_restore(key);
}

#else

/****************************************************************************
 * The transport has tape tach derived from the search-to-cue timer card
 * using the quadrature encoder from the tape timer roller. The pulse stream
 * is approximately 240 Hz with tape moving at 30 IPS. We configure
 * WTIMER1A as 32-bit input edge count mode.
  ****************************************************************************/

#define TACH_EDGE_COUNT	10

static uint32_t g_prevCount = 0xFFFFFFFF;
static uint32_t g_thisPeriod;
static uint32_t g_frequencyRawHz = 0;


void TapeTach_initialize(void)
{
    g_systemClock = SysCtlClockGet();

	/* Map the timer interrupt handlers. We don't make sys/bios calls
	 * from these interrupt handlers and there is no need to create a
	 * context handler with stack swapping for these. These handlers
	 * just update some globals variables and need to execute as
	 * quickly and efficiently as possible.
	 */
	Hwi_plug(INT_WTIMER1A, WTimer1AIntHandler);
    Hwi_plug(INT_WTIMER1B, WTimer1BIntHandler);

    /* Enable the wide timer peripheral */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER1);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);

    /* First make sure the timer is disabled */
    TimerDisable(WTIMER1_BASE, TIMER_A);
    TimerDisable(WTIMER1_BASE, TIMER_B);

    /* Disable global interrupts */
    IntMasterDisable();

    /* Enable pin PC6 for WTIMER1 WT1CCP0 */
    GPIOPinConfigure(GPIO_PC6_WT1CCP0);
    GPIOPinTypeTimer(GPIO_PORTC_BASE, GPIO_PIN_6);

	/* Configure Edge Count Timers */

    /* Configure wide timer for edge count capture, split mode. Timer-B is
     * used as a timeout timer that triggers when no edges are
     * present after half second timeout period.
     */
    TimerConfigure(WTIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_COUNT | TIMER_CFG_B_PERIODIC);

    /* Define event which generates interrupt on timer A */
    TimerControlEvent(WTIMER1_BASE, TIMER_A, TIMER_EVENT_POS_EDGE);
    /* Load set timer with the edge count to interrupt at */
	TimerLoadSet(WTIMER1_BASE, TIMER_A, TACH_EDGE_COUNT);
	/* Load the match value */
	TimerMatchSet(WTIMER1_BASE, TIMER_A, 0x0000);

    /* Configure the timeout count on timer B for half a second */
    TimerLoadSet(WTIMER1_BASE, TIMER_B, g_systemClock / 2);
    /* Enable interrupt on timer A for capture event and timer B for timeout */
    TimerIntEnable(WTIMER1_BASE, TIMER_CAPA_MATCH | TIMER_TIMB_TIMEOUT);

    /* Enable timer A & B interrupts */
    IntEnable(INT_WTIMER1A);
    IntEnable(INT_WTIMER1B);

    /* Enable master interrupts */
    IntMasterEnable();

    /* Setup standard timer1 as 32-bit periodic timer */
	TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
	/* Load timer value (TIMER_A for 32-bit operation) */
	TimerLoadSet(TIMER1_BASE, TIMER_A, 0xFFFFFFFF);

    /* Start the timers */
	TimerEnable(TIMER1_BASE, TIMER_A);
    TimerEnable(WTIMER1_BASE, TIMER_BOTH);
}

/****************************************************************************
 * WTIMER1A FALLING EDGE CAPTURE TIMER INTERRUPT HANDLER
 ****************************************************************************/
Void WTimer1AIntHandler(void)
{
    uint32_t key;
    uint32_t thisCount;

	/* Clear the interrupt */
	TimerIntClear(WTIMER1_BASE, TIMER_CAPA_MATCH);
    TimerIntClear(WTIMER1_BASE, TIMER_CAPA_EVENT);

	/* Reset the edge count and enable the timer */
	TimerLoadSet(WTIMER1_BASE, TIMER_A, TACH_EDGE_COUNT);
	TimerEnable(WTIMER1_BASE, TIMER_A);

	/* Read the current period timer count */
    thisCount = TimerValueGet(TIMER1_BASE, TIMER_A);

	/* ENTER - Critical Section */
    key = Hwi_disable();

    g_thisPeriod = g_prevCount - thisCount;

    g_prevCount = TimerValueGet(TIMER1_BASE, TIMER_A);

    /* Store RAW value, which refers to one measurement only
     * while avoiding divide by zero!
     */

    if (g_thisPeriod)
    	g_frequencyRawHz = g_systemClock / g_thisPeriod;

	/* EXIT - Critical Section */
    Hwi_restore(key);

    /* Reset half second timeout timer */
    HWREG(WTIMER1_BASE + TIMER_O_TBV) = g_systemClock / 2;
}

/****************************************************************************
 * WTIMER1B 1/2 SECOND EDGE DETECT TIMEOUT TIMER INTERRUPT HANDLER
 ****************************************************************************/

Void WTimer1BIntHandler(void)
{
    uint32_t key;

    TimerIntClear(WTIMER1_BASE, TIMER_TIMB_TIMEOUT);

    key = Hwi_disable();

    g_prevCount = 0;
    g_thisPeriod = 0;
    g_frequencyRawHz = 0;

    Hwi_restore(key);
}

/****************************************************************************
 * Read the current tape tachometer count.
 ****************************************************************************/

float TapeTach_read(void)
{
	float a;
    uint32_t key;

    key = Hwi_disable();

    //a = (float)g_systemClock / (float)g_thisPeriod;

    if (g_thisPeriod)
    	a = 800000000.0f / (float)g_thisPeriod;
    else
    	a = 0.0f;

    Hwi_restore(key);

	return a;
}

/****************************************************************************
 * Reset the tach data.
 ****************************************************************************/

void TapeTach_reset(void)
{
    uint32_t key;

    key = Hwi_disable();
    g_frequencyRawHz = 0;
    g_thisPeriod = 0;
    Hwi_restore(key);
}

#endif

/* End-Of-File */
