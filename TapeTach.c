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

static TACHDATA g_tachData;

static uint32_t g_systemClock = 1;

/* Hardware Interrupt Handlers */
static Void WTimer1AIntHandler(void);
static Void WTimer1BIntHandler(void);

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

    /* Enable interrupt on timer A for capture event */
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
    static uint32_t previousCount = 0;
    static uint32_t averageCount[TACH_AVG_QTY];
    static uint32_t averageSum = 0;
    static size_t   averageIdx = 0;

    uint32_t thisCount;
    uint32_t thisPeriod;
    uint32_t key;

    TimerIntClear(WTIMER1_BASE, TIMER_CAPA_EVENT);

    thisCount     = TimerValueGet(WTIMER1_BASE, TIMER_A);
    thisPeriod    = previousCount - thisCount;
    previousCount = thisCount;

    if (thisPeriod)       /* Shield from dividing by zero */
    {
        /* Calculates and store averaged value */
        averageSum              -= averageCount[averageIdx];
        averageSum              += thisPeriod;
        averageCount[averageIdx] = thisPeriod;
        averageIdx++;
        averageIdx %= TACH_AVG_QTY;

        /* ENTER - Critical Section */
        key = Hwi_disable();
        {
            /* Sets the status to indicate tach is alive */
            g_tachData.tachAlive = true;

            /* Store RAW value, which refers to one measurement only */
            g_tachData.frequencyRawHz = (float)g_systemClock / (float)thisPeriod;

            /* Update the average sum */
            if (averageSum)
                g_tachData.frequencyAvgHz = (float)g_systemClock / ((float)averageSum / (float)TACH_AVG_QTY);
        }
        /* EXIT - Critical Section */
        Hwi_restore(key);

        /* Resets timeout timer */
        HWREG(WTIMER1_BASE + TIMER_O_TBV) = g_systemClock / 2;
    }
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
        g_tachData.tachAlive      = false;
        g_tachData.frequencyAvgHz = 0.0f;
        g_tachData.frequencyRawHz = 0.0f;
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
	    avg = g_tachData.frequencyAvgHz;
	}
	Hwi_restore(key);

	return avg;
}

void TapeTach_reset(void)
{
    uint32_t key;

    key = Hwi_disable();
    {
        g_tachData.tachAlive      = false;
        g_tachData.frequencyAvgHz = 0.0f;
        g_tachData.frequencyRawHz = 0.0f;
    }
    Hwi_restore(key);
}

/* End-Of-File */
