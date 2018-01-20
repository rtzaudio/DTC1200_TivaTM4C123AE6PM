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

#include <stdint.h>
#include <stdbool.h>

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

#include <inc/hw_ints.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_sysctl.h>
#include <inc/hw_gpio.h>
#include <inc/hw_ssi.h>
#include <inc/hw_i2c.h>

#include <driverlib/gpio.h>
#include <driverlib/flash.h>
#include <driverlib/eeprom.h>
#include <driverlib/sysctl.h>
#include <driverlib/i2c.h>
#include <driverlib/ssi.h>

#include <driverlib/gpio.h>
#include <driverlib/i2c.h>
#include <driverlib/pin_map.h>
#include <driverlib/pwm.h>
#include <driverlib/ssi.h>
#include <driverlib/sysctl.h>
#include <driverlib/uart.h>
#include <driverlib/udma.h>
#include <driverlib/adc.h>
#include <driverlib/qei.h>

#include "DTC1200_TivaTM4C123AE6PMI.h"

#ifndef TI_DRIVERS_UART_DMA
#define TI_DRIVERS_UART_DMA 0
#endif

/*
 *  =============================== DMA ===============================
 */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(dmaControlTable, 1024)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment=1024
#elif defined(__GNUC__)
__attribute__ ((aligned (1024)))
#endif
static tDMAControlTable dmaControlTable[32];
static bool dmaInitialized = false;

/* Hwi_Struct used in the initDMA Hwi_construct call */
static Hwi_Struct dmaHwiStruct;

/*
 *  ======== dmaErrorHwi ========
 */
static Void dmaErrorHwi(UArg arg)
{
    System_printf("DMA error code: %d\n", uDMAErrorStatusGet());
    uDMAErrorStatusClear();
    System_abort("DMA error!!");
}

/*
 *  ======== DTC1200_initDMA ========
 */
void DTC1200_initDMA(void)
{
    Error_Block eb;
    Hwi_Params  hwiParams;

    if (!dmaInitialized) {
        Error_init(&eb);
        Hwi_Params_init(&hwiParams);
        Hwi_construct(&(dmaHwiStruct), INT_UDMAERR, dmaErrorHwi, &hwiParams, &eb);
        if (Error_check(&eb)) {
            System_abort("Couldn't construct DMA error hwi");
        }

        SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
        uDMAEnable();
        uDMAControlBaseSet(dmaControlTable);

        dmaInitialized = true;
    }
}

/*
 *  =============================== General ===============================
 */
 
/*
 *  ======== DTC1200_initGeneral ========
 */
void DTC1200_initGeneral(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);

	// Initialize the EEPROM so we can access it later

    SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);

    if (EEPROMInit() != EEPROM_INIT_OK)
    	System_printf("EEPROMInit() failed!\n");

    uint32_t size = EEPROMSizeGet();
}

/*
 *  =============================== GPIO ===============================
 */
 
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(GPIOTiva_config, ".const:GPIOTiva_config")
#endif

#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOTiva.h>

/* GPIO configuration structure */

/*
 * Array of Pin configurations
 * NOTE: The order of the pin configurations must coincide with what was
 *       defined in DTC1200_TM4C123AE6PMI.h
 * NOTE: Pins not used for interrupts should be placed at the end of the
 *       array.  Callback entries can be omitted from callbacks array to
 *       reduce memory usage.
 */
GPIO_PinConfig gpioPinConfigs[] = {
    /*=== Input pins ===*/
    /* (0) DTC1200_MCP23S17T_INT1A */
    GPIOTiva_PG_3 | GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING,
    /* (1) DTC1200_MCP23S17T_INT2B */
    GPIOTiva_PG_4| GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING,
	/* (2) DTC1200_TAPE_END */
    GPIOTiva_PC_7 | GPIO_CFG_INPUT,
	/* (3) DTC1200_TAPE_WIDTH */
    GPIOTiva_PG_2 | GPIO_CFG_INPUT,
	/* (4) DTC1200_MOTION_FWD */
    GPIOTiva_PF_2 | GPIO_CFG_INPUT,
	/* (5) DTC1200_MOTION_REW */
    GPIOTiva_PD_5 | GPIO_CFG_INPUT,
    /*=== Output pins ===*/
    /* (6) DTC1200_SSI0FSS */
    GPIOTiva_PA_3 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH,
    /* (7) DTC1200_SSI1FSS */
    GPIOTiva_PD_1 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH,
    /* (8 DTC1200_SSI2FSS */
    GPIOTiva_PB_5 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH,
	/* (9) DTC1200_RESET_MCP */
	GPIOTiva_PD_4 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
	/* (10) DTC1200_EXPANSION_PD5 */
	GPIOTiva_PD_5 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
	/* (11) DTC1200_EXPANSION_PF3 */
	GPIOTiva_PF_3 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
	/* (12) DTC1200_EXPANSION_PF2 */
	GPIOTiva_PF_2 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW
};

/*
 * Array of callback function pointers
 * NOTE: The order of the pin configurations must coincide with what was
 *       defined in DTC1200_TivaTM4C123AE6PMI.h
 * NOTE: Pins not used for interrupts can be omitted from callbacks array to
 *       reduce memory usage (if placed at end of gpioPinConfigs array).
 */
GPIO_CallbackFxn gpioCallbackFunctions[] = {
    NULL,  /* DTC1200_MCP23S17T_INT1A (PG4) */
    NULL   /* DTC1200_MCP23S17T_INT2B (PG3) */
};

/* The device-specific GPIO_config structure */
const GPIOTiva_Config GPIOTiva_config = {
    .pinConfigs         = (GPIO_PinConfig *)gpioPinConfigs,
    .callbacks          = (GPIO_CallbackFxn *)gpioCallbackFunctions,
    .numberOfPinConfigs = sizeof(gpioPinConfigs)/sizeof(GPIO_PinConfig),
    .numberOfCallbacks  = sizeof(gpioCallbackFunctions)/sizeof(GPIO_CallbackFxn),
    .intPriority        = (~0)
};

/*
 *  ======== DTC1200_initGPIO ========
 */
void DTC1200_initGPIO(void)
{
	// Enable pin PA3 for GPIOOutput
	GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_3);
	// Enable pin PB5 for GPIOOutput
	GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_5);
	// Enable pin PC7 for GPIOInput
	GPIOPinTypeGPIOInput(GPIO_PORTC_BASE, GPIO_PIN_7);
	// Enable pin PD4 for GPIOOutput
	GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_4);
	// Enable pin PD5 for GPIOInput
	GPIOPinTypeGPIOInput(GPIO_PORTD_BASE, GPIO_PIN_5);
	// Enable pin PD1 for GPIOOutput
	GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_1);
	// Enable pin PF3 for GPIOOutput
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3);
	// Enable pin PF2 for GPIOInput
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_2);
	// Enable pin PG4 for GPIOInput
	GPIOPinTypeGPIOInput(GPIO_PORTG_BASE, GPIO_PIN_4);
	// Enable pin PG3 for GPIOInput
	GPIOPinTypeGPIOInput(GPIO_PORTG_BASE, GPIO_PIN_3);
	// Enable pin PG2 for GPIOInput
	GPIOPinTypeGPIOInput(GPIO_PORTG_BASE, GPIO_PIN_2);

    /* Once GPIO_init is called, GPIO_config cannot be changed */
    GPIO_init();
}

/*
 *  =============================== I2C ===============================
 */
 
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(I2C_config, ".const:I2C_config")
#pragma DATA_SECTION(i2cTivaHWAttrs, ".const:i2cTivaHWAttrs")
#endif

#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CTiva.h>

/* I2C objects */
I2CTiva_Object i2cTivaObjects[DTC1200_I2CCOUNT];

/* I2C configuration structure, describing which pins are to be used */

const I2CTiva_HWAttrs i2cTivaHWAttrs[DTC1200_I2CCOUNT] = {
    {
        .baseAddr    = I2C0_BASE,
        .intNum      = INT_I2C0,
        .intPriority = (~0)
    },
    {
        .baseAddr    = I2C1_BASE,
        .intNum      = INT_I2C1,
        .intPriority = (~0)
    },
};

const I2C_Config I2C_config[] = {
    {
        .fxnTablePtr = &I2CTiva_fxnTable,
        .object      = &i2cTivaObjects[0],
        .hwAttrs     = &i2cTivaHWAttrs[0]
    },
    {
        .fxnTablePtr = &I2CTiva_fxnTable,
        .object      = &i2cTivaObjects[1],
        .hwAttrs     = &i2cTivaHWAttrs[1]
    },
    {NULL, NULL, NULL}
};

/*
 *  ======== DTC1200_initI2C ========
 */
void DTC1200_initI2C(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C1);

    // Enable pin PB2 for I2C0 I2C0SCL
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    // Enable pin PB3 for I2C0 I2C0SDA
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    // Enable pin PA7 for I2C1 I2C1SDA
    GPIOPinConfigure(GPIO_PA7_I2C1SDA);
    GPIOPinTypeI2C(GPIO_PORTA_BASE, GPIO_PIN_7);
    // Enable pin PA6 for I2C1 I2C1SCL
    GPIOPinConfigure(GPIO_PA6_I2C1SCL);
    GPIOPinTypeI2CSCL(GPIO_PORTA_BASE, GPIO_PIN_6);

    I2C_init();
}

/*
 *  =============================== SPI ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(SPI_config, ".const:SPI_config")
#pragma DATA_SECTION(spiTivaDMAHWAttrs, ".const:spiTivaDMAHWAttrs")
#endif

#include <ti/drivers/SPI.h>
#include <ti/drivers/spi/SPITivaDMA.h>

SPITivaDMA_Object spiTivaDMAObjects[DTC1200_SPICOUNT];

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(spiTivaDMAscratchBuf, 32)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment=32
#elif defined(__GNUC__)
__attribute__ ((aligned (32)))
#endif
uint32_t spiTivaDMAscratchBuf[DTC1200_SPICOUNT];

const SPITivaDMA_HWAttrs spiTivaDMAHWAttrs[DTC1200_SPICOUNT] = {
    {
        .baseAddr               = SSI0_BASE,
        .intNum                 = INT_SSI0,
        .intPriority            = (~0),
        .scratchBufPtr          = &spiTivaDMAscratchBuf[0],
        .defaultTxBufValue      = 0,
        .rxChannelIndex         = UDMA_CHANNEL_SSI0RX,
        .txChannelIndex         = UDMA_CHANNEL_SSI0TX,
        .channelMappingFxn      = uDMAChannelAssign,
        .rxChannelMappingFxnArg = UDMA_CH10_SSI0RX,
        .txChannelMappingFxnArg = UDMA_CH11_SSI0TX
    },
    {
        .baseAddr               = SSI1_BASE,
        .intNum                 = INT_SSI1,
        .intPriority            = (~0),
        .scratchBufPtr          = &spiTivaDMAscratchBuf[1],
        .defaultTxBufValue      = 0,
        .rxChannelIndex         = UDMA_CHANNEL_SSI1RX,
        .txChannelIndex         = UDMA_CHANNEL_SSI1TX,
        .channelMappingFxn      = uDMAChannelAssign,
        .rxChannelMappingFxnArg = UDMA_CH24_SSI1RX,
        .txChannelMappingFxnArg = UDMA_CH25_SSI1TX
    },
	{
        .baseAddr               = SSI2_BASE,
        .intNum                 = INT_SSI2,
        .intPriority            = (~0),
        .scratchBufPtr          = &spiTivaDMAscratchBuf[2],
        .defaultTxBufValue      = 0,
        .rxChannelIndex         = UDMA_SEC_CHANNEL_UART2RX_12,
        .txChannelIndex         = UDMA_SEC_CHANNEL_UART2TX_13,
        .channelMappingFxn      = uDMAChannelAssign,
        .rxChannelMappingFxnArg = UDMA_CH12_SSI2RX,
        .txChannelMappingFxnArg = UDMA_CH13_SSI2TX
    }
};

const SPI_Config SPI_config[] = {
    {&SPITivaDMA_fxnTable, &spiTivaDMAObjects[0], &spiTivaDMAHWAttrs[0]},
    {&SPITivaDMA_fxnTable, &spiTivaDMAObjects[1], &spiTivaDMAHWAttrs[1]},
    {&SPITivaDMA_fxnTable, &spiTivaDMAObjects[2], &spiTivaDMAHWAttrs[2]},
    {NULL, NULL, NULL},
};

/*
 *  ======== DTC1200_initSPI ========
 */
void DTC1200_initSPI(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);

    /* === Configure and Enable SSI0 === */

    // Enable pin PA4 for SSI0 SSI0RX
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_4);
    // Enable pin PA2 for SSI0 SSI0CLK
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2);
    // Enable pin PA5 for SSI0 SSI0TX
    GPIOPinConfigure(GPIO_PA5_SSI0TX);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_5);

    /* === Configure and Enable SSI1 === */

    // Enable pin PD2 for SSI1 SSI1RX
    GPIOPinConfigure(GPIO_PD2_SSI1RX);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_2);
    // Enable pin PD3 for SSI1 SSI1TX
    GPIOPinConfigure(GPIO_PD3_SSI1TX);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_3);
    // Enable pin PD0 for SSI1 SSI1CLK
    GPIOPinConfigure(GPIO_PD0_SSI1CLK);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_0);

    /* === Configure and Enable SSI2 === */

    // Enable pin PB7 for SSI2 SSI2TX
    GPIOPinConfigure(GPIO_PB7_SSI2TX);
    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_7);
    // Enable pin PB4 for SSI2 SSI2CLK
    GPIOPinConfigure(GPIO_PB4_SSI2CLK);
    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_4);
    // Enable pin PB6 for SSI2 SSI2RX
    GPIOPinConfigure(GPIO_PB6_SSI2RX);
    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_6);

    DTC1200_initDMA();
    SPI_init();
}

/*
 *  =============================== UART ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(UART_config, ".const:UART_config")
#pragma DATA_SECTION(uartTivaHWAttrs, ".const:uartTivaHWAttrs")
#endif

#include <ti/drivers/UART.h>
#if TI_DRIVERS_UART_DMA
#include <ti/drivers/uart/UARTTivaDMA.h>

UARTTivaDMA_Object uartTivaObjects[DTC1200_UARTCOUNT];

const UARTTivaDMA_HWAttrs uartTivaHWAttrs[DTC1200_UARTCOUNT] = {
    {
        .baseAddr       = UART0_BASE,
        .intNum         = INT_UART0,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH8_UART0RX,
        .txChannelIndex = UDMA_CH9_UART0TX,
    },
    {
        .baseAddr       = UART1_BASE,
        .intNum         = INT_UART1,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH22_UART1RX,
        .txChannelIndex = UDMA_CH23_UART1TX,
    },
    {
        .baseAddr       = UART5_BASE,
        .intNum         = INT_UART5,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH6_UART5RX,
        .txChannelIndex = UDMA_CH7_UART5TX,
    }
};

const UART_Config UART_config[] = {
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object      = &uartTivaObjects[0],
        .hwAttrs     = &uartTivaHWAttrs[0]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object      = &uartTivaObjects[1],
        .hwAttrs     = &uartTivaHWAttrs[1]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object      = &uartTivaObjects[2],
        .hwAttrs     = &uartTivaHWAttrs[2]
    },
    {NULL, NULL, NULL}
};
#else
#include <ti/drivers/uart/UARTTiva.h>

UARTTiva_Object uartTivaObjects[DTC1200_UARTCOUNT];
unsigned char uartTivaRingBuffer[DTC1200_UARTCOUNT][32];

/* UART configuration structure */
const UARTTiva_HWAttrs uartTivaHWAttrs[DTC1200_UARTCOUNT] = {
    {
        .baseAddr    = UART0_BASE,
        .intNum      = INT_UART0,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[0],
        .ringBufSize = sizeof(uartTivaRingBuffer[0])
    },
    {
        .baseAddr    = UART1_BASE,
        .intNum      = INT_UART1,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[1],
        .ringBufSize = sizeof(uartTivaRingBuffer[1])
    },
    {
        .baseAddr    = UART5_BASE,
        .intNum      = INT_UART5,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[2],
        .ringBufSize = sizeof(uartTivaRingBuffer[2])
    },
};

const UART_Config UART_config[] = {
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object      = &uartTivaObjects[0],
        .hwAttrs     = &uartTivaHWAttrs[0]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object      = &uartTivaObjects[1],
        .hwAttrs     = &uartTivaHWAttrs[1]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object      = &uartTivaObjects[2],
        .hwAttrs     = &uartTivaHWAttrs[2]
    },
    {NULL, NULL, NULL}
};
#endif /* TI_DRIVERS_UART_DMA */

/*
 *  ======== DTC1200_initUART ========
 */
void DTC1200_initUART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART5);

    /*
     * Enable and configure UART0
     */
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_1);

    /*
     * Enable and configure UART1
     */
    // Enable pin PF0 for UART1 U1RTS
    // First open the lock and select the bits we want to modify in the GPIO commit register.
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0x1;
    // Now modify the configuration of the pins that we unlocked.
    GPIOPinConfigure(GPIO_PF0_U1RTS);
    GPIOPinTypeUART(GPIO_PORTF_BASE, GPIO_PIN_0);
    GPIOPinConfigure(GPIO_PC5_U1TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_5);
    GPIOPinConfigure(GPIO_PF1_U1CTS);
    GPIOPinTypeUART(GPIO_PORTF_BASE, GPIO_PIN_1);
    GPIOPinConfigure(GPIO_PC4_U1RX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_4);

    /*
     * Enable and configure UART5
     */
    GPIOPinConfigure(GPIO_PE5_U5TX);
    GPIOPinTypeUART(GPIO_PORTE_BASE, GPIO_PIN_5);
    GPIOPinConfigure(GPIO_PE4_U5RX);
    GPIOPinTypeUART(GPIO_PORTE_BASE, GPIO_PIN_4);

    /* Initialize the UART driver */
#if TI_DRIVERS_UART_DMA
    DTC1200_initDMA();
#endif

    UART_init();
}

#if 0
/*
 *  =============================== PWM ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(PWM_config, ".const:PWM_config")
#pragma DATA_SECTION(pwmTivaHWAttrs, ".const:pwmTivaHWAttrs")
#endif

#include <ti/drivers/PWM.h>
#include <ti/drivers/pwm/PWMTiva.h>

PWMTiva_Object pwmTivaObjects[DTC1200_PWMCOUNT];

const PWMTiva_HWAttrs pwmTivaHWAttrs[DTC1200_PWMCOUNT] = {
    {
        .baseAddr = PWM1_BASE,
        .pwmOutput = PWM_OUT_6,
        .pwmGenOpts = PWM_GEN_MODE_DOWN | PWM_GEN_MODE_DBG_RUN
    },
    {
        .baseAddr = PWM1_BASE,
        .pwmOutput = PWM_OUT_7,
        .pwmGenOpts = PWM_GEN_MODE_DOWN | PWM_GEN_MODE_DBG_RUN
    }
};

const PWM_Config PWM_config[] = {
    {
        .fxnTablePtr = &PWMTiva_fxnTable,
        .object = &pwmTivaObjects[0],
        .hwAttrs = &pwmTivaHWAttrs[0]
    },
    {
        .fxnTablePtr = &PWMTiva_fxnTable,
        .object = &pwmTivaObjects[1],
        .hwAttrs = &pwmTivaHWAttrs[1]
    },
    {NULL, NULL, NULL}
};

/*
 *  ======== DTC1200_initPWM ========
 */
void DTC1200_initPWM(void)
{
    /* Enable PWM peripherals */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);

    /*
     * Enable PWM output on GPIO pins.  Board_LED1 and Board_LED2 are now
     * controlled by PWM peripheral - Do not use GPIO APIs.
     */
    GPIOPinConfigure(GPIO_PF2_M1PWM6);
    GPIOPinConfigure(GPIO_PF3_M1PWM7);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2 |GPIO_PIN_3);

    PWM_init();
}
#endif

/*
 *  =============================== Watchdog ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(Watchdog_config, ".const:Watchdog_config")
#pragma DATA_SECTION(watchdogTivaHWAttrs, ".const:watchdogTivaHWAttrs")
#endif

#include <ti/drivers/Watchdog.h>
#include <ti/drivers/watchdog/WatchdogTiva.h>

WatchdogTiva_Object watchdogTivaObjects[DTC1200_WATCHDOGCOUNT];

const WatchdogTiva_HWAttrs watchdogTivaHWAttrs[DTC1200_WATCHDOGCOUNT] = {
    {
        .baseAddr    = WATCHDOG0_BASE,
        .intNum      = INT_WATCHDOG,
        .intPriority = (~0),
        .reloadValue = 80000000 // 1 second period at default CPU clock freq
    },
};

const Watchdog_Config Watchdog_config[] = {
    {
        .fxnTablePtr = &WatchdogTiva_fxnTable,
        .object      = &watchdogTivaObjects[0],
        .hwAttrs     = &watchdogTivaHWAttrs[0]
    },
    {NULL, NULL, NULL},
};

/*
 *  ======== DTC1200_initWatchdog ========
 *
 * NOTE: To use the other watchdog timer with base address WATCHDOG1_BASE,
 *       an additional function call may need be made to enable PIOSC. Enabling
 *       WDOG1 does not do this. Enabling another peripheral that uses PIOSC
 *       such as ADC0 or SSI0, however, will do so. Example:
 *
 *       SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
 *       SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG1);
 *
 *       See the following forum post for more information:
 *       http://e2e.ti.com/support/microcontrollers/stellaris_arm_cortex-m3_microcontroller/f/471/p/176487/654390.aspx#654390
 */
void DTC1200_initWatchdog(void)
{
    /* Enable peripherals used by Watchdog */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG0);

    Watchdog_init();
}


/*
 *  =============================== ADC ===============================
 */

/*
 *  ======== DTC1200_initADC ========
 */

#define SAMPLE_SEQUENCER	0		/* up to 8 samples for sequencer 0 */

void DTC1200_initADC(void)
{
	/* The ADC0 peripheral must be enabled for use. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

    // Enable pin PE3 for ADC AIN0
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
    // Enable pin PE2 for ADC AIN1
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_2);
    // Enable pin PE1 for ADC AIN2
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1);
    // Enable pin PE0 for ADC AIN3
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0);

	/* The ADC0 peripheral speed. */
	//SysCtlADCSpeedSet(SYSCTL_ADCSPEED_250KSPS);

	/* Enable sample sequence 0 with a processor signal trigger. Sequence 0
	 * will do 5 samples when the processor sends a signal to start the
	 * conversion. Each ADC module has 4 programmable sequences, sequence 0
	 * to sequence 3. This example is arbitrarily using sequence 0.
	 */
	ADCSequenceConfigure(ADC0_BASE, SAMPLE_SEQUENCER, ADC_TRIGGER_PROCESSOR, 0);

	/* Configure the step sequence sources. Here we are using sample
	 * sequence zero since it supports up to eight steps. Steps 0-3
	 * are mapped to the four ADC input pins and step 4 is mapped.
	 * to the internal cpu temp sensor.
	 */

	/* Step[0] ADC2 - Tension Sensor Arm */
	ADCSequenceStepConfigure(ADC0_BASE, SAMPLE_SEQUENCER, 0, ADC_CTL_CH2);
	/* Step[1] ADC0 - Supply Motor Current Option */
	ADCSequenceStepConfigure(ADC0_BASE, SAMPLE_SEQUENCER, 1, ADC_CTL_CH0);
	/* Step[2] ADC1 - Takeup Motor Current Option */
	ADCSequenceStepConfigure(ADC0_BASE, SAMPLE_SEQUENCER, 2, ADC_CTL_CH1);
	/* Step[3] ADC3 - Expansion Port ADC input option */
	ADCSequenceStepConfigure(ADC0_BASE, SAMPLE_SEQUENCER, 3, ADC_CTL_CH3);
	/* Step[4] Internal CPU temperature sensor */
	ADCSequenceStepConfigure(ADC0_BASE, SAMPLE_SEQUENCER, 4, ADC_CTL_TS | ADC_CTL_IE | ADC_CTL_END);

	/* Since sample sequence 0 is now configured, it must be enabled. */
	ADCSequenceEnable(ADC0_BASE, SAMPLE_SEQUENCER);

	/* Clear the interrupt status flag.  This is done to make sure the
	 * interrupt flag is cleared before we sample.
	 */
	ADCIntClear(ADC0_BASE, SAMPLE_SEQUENCER);
}

/*
 * ReadADC() - Read the ADC inputs.
 *
 * unsigned long ReadADC(unsigned long *pulBuffer)
 *
 * This function fills an array of 5 long values with the ADC values
 * measured as configured by the InitADC() function above. Note the array
 * must contain at least 5 long values, on return the following is returned:
 *
 *   ulBuffer[0] = Tension sensor arm reading.
 *   ulBuffer[1] = Supply motor current reading (future option)
 *   ulBuffer[2] = Takeup motor current reading (future option)
 *   ulBuffer[3] = Expansion port ADC reading   (future option)
 *   ulBuffer[4] = Internal CPU temperature reading.
 */

int32_t DTC1200_readADC(uint32_t* pui32Buffer)
{
    /* Trigger the ADC conversion */
    ADCProcessorTrigger(ADC0_BASE, SAMPLE_SEQUENCER);

    /* Wait for conversion to be completed */
    while(!ADCIntStatus(ADC0_BASE, SAMPLE_SEQUENCER, false))
    {
    }

    /* Clear the ADC interrupt flag */
    ADCIntClear(ADC0_BASE, SAMPLE_SEQUENCER);

    /* Read ADC Values and return count */
    return ADCSequenceDataGet(ADC0_BASE, SAMPLE_SEQUENCER, pui32Buffer);
}

/* End-Of-File */
