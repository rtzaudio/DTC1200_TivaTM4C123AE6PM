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

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/gpio.h>
#include <ti/drivers/spi.h>
#include <ti/drivers/i2c.h>
#include <ti/drivers/uart.h>

#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/qei.h>

#include <inc/hw_ints.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_gpio.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Project specific includes */
#include "DTC1200.h"
#include "Globals.h"
#include "ReelQEI.h"

/* Hwi_Struct used in the init Hwi_construct call */
static Hwi_Struct qei0HwiStruct;
static Hwi_Struct qei1HwiStruct;

/* Interrupt Handlers */
static Void QEISupplyHwi(UArg arg);
static Void QEITakeupHwi(UArg arg);

/*****************************************************************************
 * QEI Configuration and interrupt handling
 *****************************************************************************/

void ReelQEI_initialize(void)
{
	Error_Block eb;
	Hwi_Params  hwiParams;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI1);

    /* Initialize the QEI quadrature encoder interface for operation. We are
     * using QEI0 for the SUPPLY reel and QEI1 for the TAKEUP reel. These are
     * both used only for velocity and direction information (not position).
     * The quadrature encoder module provides hardware encoding of the two
     * channels and the index signal from a quadrature encoder device into an
     * absolute or relative position. There is additional hardware for capturing
     * a measure of the encoder velocity, which is simply a count of encoder pulses
     * during a fixed time period; the number of pulses is directly proportional
     * to the encoder speed. Note that the velocity capture can only operate when
     * the position capture is enabled.
     */

    /* Enable pin PF4 for QEI0 IDX0 */
    GPIOPinConfigure(GPIO_PF4_IDX0);
    GPIOPinTypeQEI(GPIO_PORTF_BASE, GPIO_PIN_4);
    /* Enable pin PD7 for QEI0 PHB0. First open the lock and
     * select the bits we want to modify in the GPIO commit register.
     */
    HWREG(GPIO_PORTD_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTD_BASE + GPIO_O_CR) = 0x80;
    /* Now modify the configuration of the pins that we unlocked. */
    GPIOPinConfigure(GPIO_PD7_PHB0);
    GPIOPinTypeQEI(GPIO_PORTD_BASE, GPIO_PIN_7);
    /* Enable pin PD6 for QEI0 PHA0 */
    GPIOPinConfigure(GPIO_PD6_PHA0);
    GPIOPinTypeQEI(GPIO_PORTD_BASE, GPIO_PIN_6);
    /* Enable pin PG1 for QEI1 PHB1 */
    GPIOPinConfigure(GPIO_PG1_PHB1);
    GPIOPinTypeQEI(GPIO_PORTG_BASE, GPIO_PIN_1);
    /* Enable pin PG0 for QEI1 PHA1 */
    GPIOPinConfigure(GPIO_PG0_PHA1);
    GPIOPinTypeQEI(GPIO_PORTG_BASE, GPIO_PIN_0);
    /* Enable pin PG5 for QEI1 IDX1 */
    GPIOPinConfigure(GPIO_PG5_IDX1);
    GPIOPinTypeQEI(GPIO_PORTG_BASE, GPIO_PIN_5);

    /* Configure the quadrature encoder to capture edges on both signals and
     * maintain an absolute position by resetting on index pulses. Using a
     * 360 CPR encoder at four edges per line, there are 1440 pulses per
     * revolution; therefore set the maximum position to 1439 since the
     * count is zero based.
     */
    QEIConfigure(QEI_BASE_SUPPLY, (QEI_CONFIG_CAPTURE_A_B | QEI_CONFIG_RESET_IDX |
                 QEI_CONFIG_QUADRATURE | QEI_CONFIG_NO_SWAP), QE_EDGES_PER_REV - 1);

    QEIConfigure(QEI_BASE_TAKEUP, (QEI_CONFIG_CAPTURE_A_B | QEI_CONFIG_RESET_IDX |
                 QEI_CONFIG_QUADRATURE | QEI_CONFIG_NO_SWAP), QE_EDGES_PER_REV - 1);

    /* This function configures the operation of the velocity capture portion
     * of the quadrature encoder. The position increment signal is pre-divided
     * as specified by ulPreDiv before being accumulated by the velocity
     * capture. The divided signal is accumulated over ulPeriod system clock
     * before being saved and resetting the accumulator.
     */

    /* Configure the Velocity capture period - 800000 is 10ms at 80MHz */
    QEIVelocityConfigure(QEI_BASE_SUPPLY, QEI_VELDIV_1, QE_TIMER_PERIOD);
    QEIVelocityConfigure(QEI_BASE_TAKEUP, QEI_VELDIV_1, QE_TIMER_PERIOD);

    /* Enable both quadrature encoder interfaces */
    QEIEnable(QEI_BASE_SUPPLY);
    QEIEnable(QEI_BASE_TAKEUP);

    /* Enable both quadrature velocity capture interfaces. */
    QEIVelocityEnable(QEI_BASE_SUPPLY);
    QEIVelocityEnable(QEI_BASE_TAKEUP);

    /* Now we construct the interrupt handler objects for TI-RTOS */

    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&(qei0HwiStruct), INT_QEI0, QEISupplyHwi, &hwiParams, &eb);
    if (Error_check(&eb)) {
        System_abort("Couldn't construct DMA error hwi");
    }

    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&(qei1HwiStruct), INT_QEI1, QEITakeupHwi, &hwiParams, &eb);
    if (Error_check(&eb)) {
        System_abort("Couldn't construct DMA error hwi");
    }

    QEIIntEnable(QEI_BASE_SUPPLY, QEI_INTERROR|QEI_INTTIMER);
    QEIIntEnable(QEI_BASE_TAKEUP, QEI_INTERROR);
}

/*****************************************************************************
 * QEI Interrupt Handlers
 *****************************************************************************/

Void QEISupplyHwi(UArg arg)
{
	UInt key;
    unsigned long ulIntStat;

    /* Get and clear the current interrupt source(s) */
    ulIntStat = QEIIntStatus(QEI_BASE_SUPPLY, true);
    QEIIntClear(QEI_BASE_SUPPLY, ulIntStat);

    /* Determine which interrupt occurred */

    if (ulIntStat & QEI_INTERROR)       	/* phase error detected */
    {
    	key = Hwi_disable();
    	g_servo.qei_supply_error_cnt++;
    	Hwi_restore(key);
    }
    else if (ulIntStat & QEI_INTTIMER)  	/* velocity timer expired */
    {
        GPIO_toggle(DTC1200_EXPANSION_PF2);
    }
    else if (ulIntStat & QEI_INTDIR)    	/* direction change */
    {

    }
    else if (ulIntStat & QEI_INTINDEX)  	/* Index pulse detected */
    {

    }

    QEIIntEnable(QEI_BASE_SUPPLY, ulIntStat);
}

Void QEITakeupHwi(UArg arg)
{
	UInt key;
    unsigned long ulIntStat;

    /* Get and clear the current interrupt source(s) */
    ulIntStat = QEIIntStatus(QEI_BASE_TAKEUP, true);
    QEIIntClear(QEI_BASE_TAKEUP, ulIntStat);

    /* Determine which interrupt occurred */

    if (ulIntStat & QEI_INTERROR)       	/* phase error detected */
    {
    	key = Hwi_disable();
    	g_servo.qei_takeup_error_cnt++;
    	Hwi_restore(key);
    }
    else if (ulIntStat & QEI_INTTIMER)  	/* velocity timer expired */
    {

    }
    else if (ulIntStat & QEI_INTDIR)    	/* direction change */
    {

    }
    else if (ulIntStat & QEI_INTINDEX)  	/* Index pulse detected */
    {

    }

    QEIIntEnable(QEI_BASE_TAKEUP, ulIntStat);
}

// End-Of-File
