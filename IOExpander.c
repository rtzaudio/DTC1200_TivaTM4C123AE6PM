/****************************************************************************
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016, RTZ Professional Audio LLC
 *
 * The source code contained in this file and associated files is copyright
 * protected material. This material contains the confidential, proprietary
 * and trade secret information of Sigma Software Research. No disclosure or
 * use of any portions of this material may be made without the express
 * written consent of RTZ Professional Audio.
 *****************************************************************************/

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//#include <driverlib/sysctl.h>

/* XDCtools Header files */
#include "Board.h"
#include "DTC1200.h"
#include "Globals.h"
#include "IOExpander.h"

/* Semaphore timeout 100ms */

extern Semaphore_Handle g_semaSPI;

/*****************************************************************************
 * I/O handle and configuration data
 *****************************************************************************/

#define NUM_OBJ	2

static IOExpander_InitData initData_SPI1[] = {
    { MCP_IOCONA, C_SEQOP },	/* Configure for byte mode */
    { MCP_IODIRA, 0xFF },	    /* Port A - all inputs from transport switches */
    { MCP_IODIRB, 0x00 },		/* Port B - all outputs to lamp/led drivers */
    { MCP_IOPOLA, 0x40 },		/* Invert input polarity of tape-out switch */
};

static IOExpander_InitData initData_SPI2[] = {
    { MCP_IOCONA, C_SEQOP },	/* Configure for byte mode */
    { MCP_IODIRA, 0x00 },	    /* Port A - solenoid and other drivers, all outputs */
    { MCP_IODIRB, 0xFF },		/* Port B - DIP switches and tape-speed switch, all inputs. */
    { MCP_IOPOLB, 0x8F },		/* Invert input polarity of DIP switches and tape-speed switch */
};

#define NCOUNT(d)	( sizeof(d)/sizeof(IOExpander_InitData) )

/* I/O Expander Handle Data */
static IOExpander_Object IOExpanderObjects[NUM_OBJ] = {
	{ NULL, Board_SPI1, Board_CS_SPI1, initData_SPI1, NCOUNT(initData_SPI1) },
	{ NULL, Board_SPI2, Board_CS_SPI2, initData_SPI2, NCOUNT(initData_SPI2) },
};

/* SPI I/O Expander Handles */
static IOExpander_Handle g_handleSPI1;
static IOExpander_Handle g_handleSPI2;

/*****************************************************************************
 * Static Function Prototypes
 *****************************************************************************/

static bool MCP23S17_write(
	IOExpander_Handle	handle,
    uint8_t   			ucRegAddr,
    uint8_t   			ucData
    );

static bool MCP23S17_read(
	IOExpander_Handle	handle,
    uint8_t				ucRegAddr,
    uint8_t*			pucData
    );

static IOExpander_Handle IOExpander_open(uint32_t index);

/*****************************************************************************
 * PUBLIC FUNCTIONS
 *****************************************************************************/
void IOExpander_initialize(void)
{
	/* Reset low pulse to I/O expanders */
	GPIO_write(Board_RESET_MCP, PIN_LOW);
    Task_sleep(100);
	GPIO_write(Board_RESET_MCP, PIN_HIGH);
    Task_sleep(100);

	/* Open the SPI 1 port */
	g_handleSPI1 = IOExpander_open(0);
	if (g_handleSPI1 == NULL)
		System_abort("Error opening SPI1\n");

	/* Open the SPI 2 port */
	g_handleSPI2 = IOExpander_open(1);
	if (g_handleSPI2 == NULL)
		System_abort("Error opening SPI2\n");
}

/*****************************************************************************
 * Read the transport control switches, returns any of the following bits:
 *
 *  S_STOP   - stop button
 *  S_PLAY   - play button
 *  S_REC    - record button
 *  S_LDEF   - lift defeat button
 *  S_FWD    - fast fwd button
 *  S_REW    - rewind button
 *  S_TOUT   - tape out switch
 *****************************************************************************/

uint32_t GetTransportSwitches(uint8_t* pucMask)
{
	uint32_t rc = 0;

	/* Acquire the semaphore for exclusive access */
    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
		MCP23S17_read(g_handleSPI1, MCP_GPIOA, pucMask);

    	Semaphore_post(g_semaSPI);
    }

    return rc;
}

/*****************************************************************************
 * Read the DIP mode configuration switch settings. This also returns
 * the tape 15/30 IPS speed switch setting.
 *
 *  M_DIPSW1    - config switch 1
 *  M_DIPSW2    - config switch 2
 *  M_DIPSW3    - config switch 3
 *  M_DIPSW4    - config switch 4
 *  M_HISPEED   - remote speed select switch
 *****************************************************************************/

uint32_t GetModeSwitches(uint8_t* pucMask)
{
	uint32_t rc = 0;

	/* Acquire the semaphore for exclusive access */
    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
		MCP23S17_read(g_handleSPI2, MCP_GPIOB, pucMask);

    	Semaphore_post(g_semaSPI);
    }

    return rc;
}

/*****************************************************************************
 * Set transport solenoids and/or record pulse/hold output drivers.
 * The following bitmasks are valid:
 *
 *  T_SERVO     - capstan servo enable
 *  T_BRAKE     - engage brakes (release solenoid)
 *  T_TLIFT     - engage tape lifter solenoid
 *  T_PROL      - engage pinch roller solenoid
 *  T_RECP      - record pulse toggle bit
 *  T_RECH      - record hold bit
 *****************************************************************************/

/* Current transport output state bits */
static uint8_t s_ucTransportMask = 0;

uint32_t SetTransportMask(uint8_t ucSetMask, uint8_t ucClearMask)
{
	uint32_t rc = 0;

	/* Acquire the semaphore for exclusive access */
    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
    	/* Clear any bits in the clear mask */
    	s_ucTransportMask &= ~(ucClearMask);

    	/* Set any bits in the set mask */
    	s_ucTransportMask |= ucSetMask;

    	/* Set the GPIO pin mask on the MCP I/O expander */
    	MCP23S17_write(g_handleSPI2, MCP_GPIOA, s_ucTransportMask);

    	Semaphore_post(g_semaSPI);
    }

    return rc;
}

uint8_t GetTransportMask(void)
{
	uint8_t mask = 0;

    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
    	mask = s_ucTransportMask;

    	Semaphore_post(g_semaSPI);
    }

    return mask;
}

/*****************************************************************************
 * Set lamp & led indicators. The following bitmasks are valid:
 *
 *  L_FWD   - forward indicator lamp
 *  L_REW   - rewind indicator lamp
 *  L_PLAY  - play indicator lamp
 *  L_REC   - record indicator lamp
 *  L_STOP  - stop indicator lamp
 *  L_LED3  - diagnostic led3
 *  L_LED2  - diagnostic led2
 *  L_LED1  - diagnostic led1
 *****************************************************************************/

/* Current transport output state bits */
static uint8_t s_ucLampMask = 0;

uint32_t SetLamp(uint8_t ucBitMask)
{
	uint32_t rc = 0;

	/* Acquire the semaphore for exclusive access */
    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
    	s_ucLampMask = ucBitMask;

    	MCP23S17_write(g_handleSPI1, MCP_GPIOB, ucBitMask);

    	Semaphore_post(g_semaSPI);
    }

    return rc;
}

uint32_t SetLampMask(uint8_t ucSetMask, uint8_t ucClearMask)
{
	uint32_t rc = 0;

	/* Acquire the semaphore for exclusive access */
    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
    	/* Clear any bits in the clear mask */
    	s_ucLampMask &= ~(ucClearMask);

    	/* Set any bits in the set mask */
    	s_ucLampMask |= ucSetMask;

    	MCP23S17_write(g_handleSPI1, MCP_GPIOB, s_ucLampMask);

    	Semaphore_post(g_semaSPI);
    }

    return rc;
}

uint8_t GetLampMask(void)
{
	uint8_t mask = 0;

    if (Semaphore_pend(g_semaSPI, TIMEOUT_SPI))
    {
    	mask = s_ucLampMask;

    	Semaphore_post(g_semaSPI);
    }

    return mask;
}

/*****************************************************************************
 * STATIC PRIVATE FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * Initialize the MCP23017 I/O Expander Chips U5 on SS1 and U8 on SSI2
 *****************************************************************************/

IOExpander_Handle IOExpander_open(uint32_t index)
{
	uint32_t i, key;
	IOExpander_Handle handle;
	IOExpander_InitData* initData;
	SPI_Params	spiParams;

	handle = &(IOExpanderObjects[index]);

	/* Determine if the device index was already opened */
	key = Hwi_disable();
	if (handle->spiHandle) {
		Hwi_restore(key);
		return NULL;
	}
	Hwi_restore(key);

	SPI_Params_init(&spiParams);

	spiParams.transferMode	= SPI_MODE_BLOCKING;
	spiParams.mode 			= SPI_MASTER;
	spiParams.frameFormat 	= SPI_POL0_PHA0;
	spiParams.bitRate 		= 1000000;
	spiParams.dataSize 		= 8;

	/* Open SPI driver to the IO Expander */
	handle->spiHandle = SPI_open(handle->boardSPI, &spiParams);

	if (handle->spiHandle == NULL) {
		System_printf("Error opening I/O Expander SPI port\n");
		return NULL;
	}

	/* Initialize the I/O expander */

	initData = IOExpanderObjects[index].initData;

	for (i=0; i < IOExpanderObjects[index].initDataCount; i++)
	{
		MCP23S17_write(handle, initData->addr, initData->data);
	     ++initData;
	}

	return handle;
}

/*****************************************************************************
 * Write a register command byte to MCP23S17 expansion I/O controller.
 *****************************************************************************/

bool MCP23S17_write(
	IOExpander_Handle	handle,
    uint8_t   			ucRegAddr,
    uint8_t   			ucData
    )
{
	SPI_Transaction opcodeTransaction;
	SPI_Transaction dataTransaction;
	uint8_t txBuffer[2];
	uint8_t rxBuffer[2];
	uint8_t dummyBuffer = 0;

	txBuffer[0] = 0x40;			/* write opcode */
	txBuffer[1] = ucRegAddr;	/* register address */

	/* Initialize opcode transaction structure */
	opcodeTransaction.count = 2;
	opcodeTransaction.txBuf = (Ptr)&txBuffer;
	opcodeTransaction.rxBuf = (Ptr)&rxBuffer;

	/* Initialize data transaction structure */
	dataTransaction.count = 1;
	dataTransaction.txBuf = (Ptr)&ucData;
	dataTransaction.rxBuf = (Ptr)&dummyBuffer;

	/* Hold SPI chip select low */
	GPIO_write(handle->boardCS, PIN_LOW);

	/* Initiate SPI transfer of opcode */
	if(!SPI_transfer(handle->spiHandle, &opcodeTransaction)) {
	    System_printf("Unsuccessful master SPI transfer to MCP23S17");
	    return false;
	}

	/* Initiate SPI transfer of data */
	if(!SPI_transfer(handle->spiHandle, &dataTransaction)) {
	    System_printf("Unsuccessful master SPI transfer to MCP23S17");
	    return false;
	}

	/* Release SPI chip select */
	GPIO_write(handle->boardCS, PIN_HIGH);

	return true;
}

/*****************************************************************************
 * Read a register command byte from MCP23S17 expansion I/O controller.
 *****************************************************************************/

bool MCP23S17_read(
	IOExpander_Handle	handle,
    uint8_t				ucRegAddr,
    uint8_t*			pucData
    )
{
	SPI_Transaction opcodeTransaction;
	SPI_Transaction dataTransaction;
	uint8_t txBuffer[2];
	uint8_t rxBuffer[2];
	uint8_t dummyBuffer = 0;

	txBuffer[0] = 0x41;			/* read opcode */
	txBuffer[1] = ucRegAddr;	/* register address */

	/* Initialize opcode transaction structure */
	opcodeTransaction.count = 2;
	opcodeTransaction.txBuf = (Ptr)&txBuffer;
	opcodeTransaction.rxBuf = (Ptr)&rxBuffer;

	/* Initialize data transaction structure */
	dataTransaction.count = 1;
	dataTransaction.txBuf = (Ptr)&dummyBuffer;
	dataTransaction.rxBuf = (Ptr)pucData;

	/* Hold SPI chip select low */
	GPIO_write(handle->boardCS, PIN_LOW);

	/* Initiate SPI transfer of opcode */
	if(!SPI_transfer(handle->spiHandle, &opcodeTransaction)) {
	    System_printf("Unsuccessful master SPI transfer to MCP23S17");
	    return false;
	}

	/* Initiate SPI transfer of data */
	if(!SPI_transfer(handle->spiHandle, &dataTransaction)) {
	    System_printf("Unsuccessful master SPI transfer to MCP23S17");
	    return false;
	}

	/* Release SPI chip select */
	GPIO_write(handle->boardCS, PIN_HIGH);

	return true;
}

// End-Of-File
