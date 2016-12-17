/*
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
 */
/** ============================================================================
 *  @file       DTC1200.h
 *
 *  @brief      DTC1200 Board Specific APIs
 *
 *  The DTC1200 header file should be included in an application as follows:
 *  @code
 *  #include <DTC1200.h>
 *  @endcode
 *
 *  ============================================================================
 */

#ifndef __DTC1200_TM4C123AE6PMI_H
#define __DTC1200_TM4C123AE6PMI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ti/drivers/GPIO.h>

/* LEDs on DTC1200 are active high. */
#define DTC1200_LED_OFF ( 0)
#define DTC1200_LED_ON  (~0)

#define PIN_LOW			( 0)
#define PIN_HIGH		(~0)

/*** Hardware Constants ****************************************************/

#define DAC_MIN             0           	/* zero scale dac setting  */
#define DAC_MAX             0x03FF        	/* 10-bit full scale DAC   */

#define ADC_MIN             0           	/* zero scale adc input    */
#define ADC_MAX             0x03FF      	/* full scale adc input    */

#define QEI_BASE_SUPPLY	    QEI0_BASE   	/* QEI-0 is SUPPLY encoder */
#define QEI_BASE_TAKEUP	    QEI1_BASE   	/* QEI-1 is TAKEUP encoder */

/*******************************************************************************
 * MCP23017 I/O Expander U5 Lamp Definitions
 ******************************************************************************/

/* Transport Switch Inputs */
#define S_STOP          0x01        // stop button
#define S_PLAY          0x02        // play button
#define S_REC           0x04        // record button
#define S_REW           0x08        // rewind button
#define S_FWD           0x10        // fast fwd button
#define S_LDEF          0x20        // lift defeat button
#define S_TAPEOUT       0x40      	// tape out switch
#define S_TAPEIN        0x80      	// tape detect (dummy bit)

#define S_BUTTON_MASK   (S_STOP | S_PLAY | S_REC | S_LDEF | S_FWD | S_REW)
#define S_SWITCH_MASK   (S_TAPEOUT)

/* Lamp Driver Outputs */
#define L_REC           0x01      	// record indicator lamp
#define L_PLAY          0x02      	// play indicator lamp
#define L_STOP          0x04      	// stop indicator lamp
#define L_FWD           0x08      	// forward indicator lamp
#define L_REW           0x10      	// rewind indicator lamp
#define L_STAT1         0x20      	// diagnostic led1
#define L_STAT2         0x40      	// diagnostic led2
#define L_STAT3         0x80   		// diagnostic led3

#define L_LED_MASK      (L_STAT3 | L_STAT2 | L_STAT1)
#define L_LAMP_MASK     (L_FWD | L_REW | L_PLAY | L_REC | L_STOP)

/*******************************************************************************
 * MCP23017 I/O Expander U8 Solenoid Drivers and Mode Configuration Switches
 ******************************************************************************/

/* Transport Solenoid and Record drivers */
#define T_BRAKE      	0x01        // engage reel motor brakes
#define T_TLIFT         0x02        // engage tape lifter solenoid
#define T_PROL          0x04        // engage pinch roller solenoid
#define T_RECP          0x08        // record pulse toggle bit
#define T_RECH          0x10        // record hold bit
#define T_SERVO         0x20        // capstan servo enable

#define T_REC_MASK      (T_RECP | T_RECH)

/* Config DIP Switches & speed select */
#define M_DIPSW1        0x01        // config DIP switch 1
#define M_DIPSW2        0x02        // config DIP switch 2
#define M_DIPSW3        0x04        // config DIP switch 3
#define M_DIPSW4        0x08        // config DIP switch 4
#define M_HISPEED       0x80        // remote speed select switch

#define M_DIPSW_MASK    (M_DIPSW1 | M_DIPSW2 | M_DIPSW3 | M_DIPSW4)

/*******************************************************************************
 * MCP23017 Register Addresses (IOCON.BANK = 0)
 ******************************************************************************/

#define MCP_IODIRA      0x00
#define MCP_IODIRB      0x01
#define MCP_IOPOLA      0x02
#define MCP_IOPOLB      0x03
#define MCP_GPINTENA    0x04
#define MCP_GPINTENB    0x05
#define MCP_DEFVALA     0x06
#define MCP_DEFVALB     0x07
#define MCP_INTCONA     0x08
#define MCP_INTCONB     0x09
#define MCP_IOCONA      0x0A
#define MCP_IOCONB      0x0B
#define MCP_GPPUA       0x0C
#define MCP_GPPUB       0x0D
#define MCP_INTFA       0x0E
#define MCP_INTFB       0x0F
#define MCP_INTCAPA     0x10
#define MCP_INTCAPB     0x11
#define MCP_GPIOA       0x12
#define MCP_GPIOB       0x13
#define MCP_OLATA       0x14
#define MCP_OLATB       0x15

/* IOCON Configuration Register Bits */
#define C_INTPOL        0x02	/* INT output 1=Active-high, 0=Active-low. */
#define C_ODR           0x04	/* INT pin as an open-drain output         */
#define C_HAEN          0x08	/* Hardware address enable (N/A for I2C)   */
#define C_DISSLW        0x10	/* Slew rate disable bit                   */
#define C_SEQOP         0x20	/* Disable address pointer auto-increment  */
#define C_MIRROR        0x40	/* INT A/B pins mirrored                   */
#define C_BANK          0x80	/* port registers are in different banks   */

/*
 * Tape moves at 30 IPS in high speed and 15 IPS in low speed. We want to
 * sample and correct at 0.3" intervals so our sample rate needs to
 * be 100 Hz (30/0.3 = 100). The timer delay period required in
 * milliseconds is 1/Hz * 1000. Therefore 1/100*1000 = 10ms. Therefore,
 * if we to correct at 0.15" intervals, the sample period would be 5ms.
 */

//#define SAMPLE_PERIOD_30_IPS	10
//#define SAMPLE_PERIOD_15_IPS	20

/* This macro is used to calculate the RPM of the motor based on velocity.
 * Our encoders are US Digital Model H1-360-I with 360 cycles per revolution.
 * We configure the QEI to capture edges on both signals and maintain an
 * absolute angular position by resetting on index pulses. So, our 360 CPR
 * encoder at four edges per line, gives us 1440 pulses per revolution.
 *
 * The period of the timer is configurable by specifying the load value
 * for the timer in the QEILOAD register. We can calculate RPM with
 * the following equation:
 *
 *	RPM = (clock * (2 ^ VelDiv) * Speed * 60) / (Load * PPR * Edges)
 *
 * For our case, consider a motor running at 600 rpm. A 360 pulse per
 * revolution quadrature encoder is attached to the motor, producing 1440
 * phase edges per revolution. With a velocity pre-divider of �1
 * (VelDiv set to 0) and clocking on both PhA and PhB edges, this results
 * in 14,400 pulses per second (the motor turns 10 times per second).
 * If the timer were clocked at 50,000,000 Hz, and the load value was
 * 12,500,000 (1/4 of a second), it would count 14400 pulses per update.
 *
 *	RPM = (50,000,000 * 1 * s * 60) / (500,000 * 360 * 4) = 600 rpm
 *	RPM = (100 * s) / 24 = 600 rpm
 *	RPM = (25 * s) / 6 = 600 rpm
 *
 *	RPM = (50,000,000 * 1 * s * 60) / (5,000,000 * 360 * 4) = 600 rpm
 *	RPM = (10 * s) / 24 = 600 rpm
 */

#define QE_EDGES_PER_REV	(360 * 4)	/* 360 * 4 for four quad encoder edges */
#define QE_TIMER_PERIOD		500000		/* period of 500,000 is 10ms at 50MHz  */

/* Calculate RPM from the velocity value */
#if (QE_TIMER_PERIOD == 500000)
	#define RPM(t)				((25 * t) / 6)
#elif (QE_TIMER_PERIOD == 5000000)
	#define RPM(t)				((10 * t) / 24)
#else
	#pragma(error, "Invalid QE_TIMER_PERIOD Specified!")
#endif


/*!
 *  @def    DTC1200_GPIOName
 *  @brief  Enum of LED names on the DTC1200 dev board
 */
typedef enum DTC1200_GPIOName {
	DTC1200_MCP23S17T_INT1A = 0,	/* PG4 */
	DTC1200_MCP23S17T_INT2B,		/* PG3 */
	DTC1200_TAPE_END,				/* PC7 */
	DTC1200_TAPE_WIDTH,				/* PG2 */
	DTC1200_SSI0FSS,				/* PA3 */
	DTC1200_SSI1FSS,				/* PD1 */
	DTC1200_SSI2FSS,				/* PB5 */
	DTC1200_EXPANSION_PC6,			/* PC6 */
	DTC1200_RESET_MCP,				/* PD4 */
	DTC1200_EXPANSION_PD5,			/* PD5 */
	DTC1200_EXPANSION_PF3,			/* PF3 */
	DTC1200_EXPANSION_PF2,			/* PF2 */

    DTC1200_GPIOCOUNT
} DTC1200_GPIOName;

/*!
 *  @def    DTC1200_I2CName
 *  @brief  Enum of I2C names on the DTC1200 dev board
 */
typedef enum DTC1200_I2CName {
    DTC1200_I2C0 = 0,
    DTC1200_I2C1,

    DTC1200_I2CCOUNT
} DTC1200_I2CName;

/*!
 *  @def    DTC1200_PWMName
 *  @brief  Enum of PWM names on the DTC1200 dev board
 */
typedef enum DTC1200_PWMName {
    DTC1200_PWM0 = 0,

    DTC1200_PWMCOUNT
} DTC1200_PWMName;

/*!
 *  @def    DTC1200_SPIName
 *  @brief  Enum of SPI names on the DTC1200 dev board
 */
typedef enum DTC1200_SPIName {
    DTC1200_SPI0 = 0,		/* SUPPLY & TAKEUP SERVO DAC */
    DTC1200_SPI1,			/* MCP23S17SO TRANSPORT SWITCHES & LAMPS */
    DTC1200_SPI2,			/* MCP23S17SO SOLENOID, CONFIG DIP SWITCH & TAPE OUT */

    DTC1200_SPICOUNT
} DTC1200_SPIName;

/*!
 *  @def    DTC1200_UARTName
 *  @brief  Enum of UARTs on the DTC1200 dev board
 */
typedef enum DTC1200_UARTName {
    DTC1200_UART0 = 0,		/* RS232 to/from STC-1200 board */
    DTC1200_UART1,			/* COM1 serial port */
    DTC1200_UART5,			/* RS232 tx/rc to expansion connector */

    DTC1200_UARTCOUNT
} DTC1200_UARTName;

/*
 *  @def    DTC1200_WatchdogName
 *  @brief  Enum of Watchdogs on the DTC1200 dev board
 */
typedef enum DTC1200_WatchdogName {
    DTC1200_WATCHDOG0 = 0,

    DTC1200_WATCHDOGCOUNT
} DTC1200_WatchdogName;

/*!
 *  @brief  Initialize the general board specific settings
 *
 *  This function initializes the general board specific settings. This include
 *     - Enable clock sources for peripherals
 */
extern void DTC1200_initGeneral(void);

/*!
 *  @brief  Initialize board specific GPIO settings
 *
 *  This function initializes the board specific GPIO settings and
 *  then calls the GPIO_init API to initialize the GPIO module.
 *
 *  The GPIOs controlled by the GPIO module are determined by the GPIO_config
 *  variable.
 */
extern void DTC1200_initGPIO(void);

/*!
 *  @brief  Initialize board specific I2C settings
 *
 *  This function initializes the board specific I2C settings and then calls
 *  the I2C_init API to initialize the I2C module.
 *
 *  The I2C peripherals controlled by the I2C module are determined by the
 *  I2C_config variable.
 */
extern void DTC1200_initI2C(void);

/*!
 *  @brief  Initialize board specific PWM settings
 *
 *  This function initializes the board specific PWM settings and then calls
 *  the PWM_init API to initialize the PWM module.
 *
 *  The PWM peripherals controlled by the PWM module are determined by the
 *  PWM_config variable.
 */
extern void DTC1200_initPWM(void);

/*!
 *  @brief  Initialize board specific SPI settings
 *
 *  This function initializes the board specific SPI settings and then calls
 *  the SPI_init API to initialize the SPI module.
 *
 *  The SPI peripherals controlled by the SPI module are determined by the
 *  SPI_config variable.
 */
extern void DTC1200_initSPI(void);

/*!
 *  @brief  Initialize board specific UART settings
 *
 *  This function initializes the board specific UART settings and then calls
 *  the UART_init API to initialize the UART module.
 *
 *  The UART peripherals controlled by the UART module are determined by the
 *  UART_config variable.
 */
extern void DTC1200_initUART(void);

/*!
 *  @brief  Initialize board specific Watchdog settings
 *
 *  This function initializes the board specific Watchdog settings and then
 *  calls the Watchdog_init API to initialize the Watchdog module.
 *
 *  The Watchdog peripherals controlled by the Watchdog module are determined
 *  by the Watchdog_config variable.
 */
extern void DTC1200_initWatchdog(void);


extern void DTC1200_initQEI(void);
extern void DTC1200_initADC(void);
extern int32_t DTC1200_readADC(uint32_t* pui32Buffer);

#ifdef __cplusplus
}
#endif

#endif /* __DTC1200_TM4C123AE6PMI_H */