/******************************************************************************
 *
 * Default Linker Command file for the Texas Instruments TM4C123AE6PM
 *
 * This is derived from revision 15071 of the TivaWare Library.
 *
 *****************************************************************************/

--retain=g_pfnVectors

/*
 * Settings for standard compilation.
 */

#define APP_BASE 0x00000000
#define APP_LENG 0x00020000

/*
 * Settings for use with bootloader compilation (app base is offset, app length is smaller)
 *
 * NOTE - for TI-RTOS you must also edit the XDC CFG file and add the following
 *
 *     Program.sectMap[".resetVecs"].loadAddress = 4096;
 *
 * The APP_BASE and the resetVecs parameters must match for the bootloader to enter
 * our application snf interrupt vectors at the proper address. We've allowed
 * 4k space for our bootloader and the application starts at this offset.
 */

//#define	APP_BASE 0x00001000
//#define	APP_LENG 0x0001F000

#define	RAM_BASE 0x20000000
#define RAM_LENG 0x00008000

MEMORY
{
    FLASH (RX) : origin = APP_BASE, length = APP_LENG
    SRAM (RWX) : origin = RAM_BASE, length = RAM_LENG
}

/* The following command line options are set as part of the CCS project.    */
/* If you are building using the command line, or for some reason want to    */
/* define them here, you can uncomment and modify these lines as needed.     */
/* If you are using CCS for building, it is probably better to make any such */
/* modifications in your CCS project and leave this file alone.              */
/*                                                                           */
/* --heap_size=0                                                             */
/* --stack_size=256                                                          */
/* --library=rtsv7M4_T_le_eabi.lib                                           */

/* Section allocation in memory */

SECTIONS
{
    .intvecs:   > APP_BASE
    .text   :   > FLASH
    .const  :   > FLASH
    .cinit  :   > FLASH
    .pinit  :   > FLASH
    .init_array : > FLASH

    .vtable :   > RAM_BASE
    .data   :   > SRAM
    .bss    :   > SRAM
    .sysmem :   > SRAM
    .stack  :   > SRAM
}

__STACK_TOP = __stack + 512;
