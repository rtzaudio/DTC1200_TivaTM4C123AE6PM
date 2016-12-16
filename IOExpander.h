#ifndef DTC1200_TIVATM4C123AE6PMI_IOEXPANDER_H_
#define DTC1200_TIVATM4C123AE6PMI_IOEXPANDER_H_

typedef struct IOExpander_InitData {
    uint8_t	addr;
    uint8_t data;
} IOExpander_InitData;

typedef struct IOExpander_Object {
    SPI_Handle      		spiHandle;	/* Handle for SPI object */
    uint32_t    			boardSPI; 	/* Board SPI in Board.h */
    uint32_t    			boardCS;  	/* Board chip select in Board.h */
    IOExpander_InitData* 	initData;
    uint32_t				initDataCount;
} IOExpander_Object;

typedef IOExpander_Object *IOExpander_Handle;

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

void IOExpander_initialize(void);

// These functions access the I/O Expanders

uint32_t GetTransportSwitches(uint8_t* pucMask);
uint32_t GetModeSwitches(uint8_t* pucMask);

uint32_t SetTransportMask(uint8_t ucSetMask, uint8_t ucClearMask);
uint8_t GetTransportMask(void);

uint32_t SetLamp(uint8_t ucBitMask);
uint32_t SetLampMask(uint8_t ucSetMask, uint8_t ucClearMask);
uint8_t GetLampMask(void);

#endif /*DTC1200_TIVATM4C123AE6PMI_IOEXPANDER_H_*/
