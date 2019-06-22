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
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Queue.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* Tivaware Driver files */
#include <driverlib/eeprom.h>
#include <driverlib/fpu.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* XDCtools Header files */
#include "Board.h"
#include "DTC1200.h"
#include "Globals.h"
#include "IOExpander.h"
#include "Utils.h"

/* External Data Items */

//extern Semaphore_Handle g_semaSPI;
//extern Semaphore_Handle g_semaServo;
//extern Semaphore_Handle g_semaTransportMode;

//extern IOExpander_Handle g_handleSPI1;
//extern IOExpander_Handle g_handleSPI2;

//*****************************************************************************
// This function attempts to read the unique serial number from
// the AT24CS01 I2C serial EPROM and serial# number device.
//*****************************************************************************

bool ReadSerialNumber(I2C_Handle handle, uint8_t ui8SerialNumber[16])
{
    bool            ret;
    uint8_t         txByte;
    I2C_Transaction i2cTransaction;

    /* default invalid serial number is all FF's */
    memset(ui8SerialNumber, 0xFF, sizeof(ui8SerialNumber));

    /* Note the Upper bit of the word address must be set
     * in order to read the serial number. Thus 80H would
     * set the starting address to zero prior to reading
     * this sixteen bytes of serial number data.
     */

    txByte = 0x80;

    i2cTransaction.slaveAddress = Board_AT24CS01_SERIAL_ADDR;
    i2cTransaction.writeBuf     = &txByte;
    i2cTransaction.writeCount   = 1;
    i2cTransaction.readBuf      = ui8SerialNumber;
    i2cTransaction.readCount    = 16;

    ret = I2C_transfer(handle, &i2cTransaction);

    if (!ret)
    {
        System_printf("Unsuccessful I2C transfer\n");
        System_flush();
    }

    return ret;
}

//*****************************************************************************
// Chase through the three status LED's for successful boot indication.
//*****************************************************************************

void FlashLEDSuccess(void)
{
    UInt32 delay = 100;
    SetLamp(L_STAT1);
    Task_sleep(delay);
    SetLamp(L_STAT2);
    Task_sleep(delay);
    SetLamp(L_STAT3);
    Task_sleep(delay);
    SetLamp(L_STAT2);
    Task_sleep(delay);
    SetLamp(L_STAT1);
    Task_sleep(delay);
    SetLamp(0);
}

//*****************************************************************************
// Flash all three status LED's to indicate configuration parameter load error.
//*****************************************************************************

void FlashLEDError(void)
{
    int i;

    /* All lamps off */
    SetLamp(0);

    /* Flash record LED and all three status LED's on error */
    for (i=0; i < 5; i++)
    {
        SetLamp(L_STAT1 | L_STAT2| L_STAT3);
        Task_sleep(200);
        SetLamp(0);
        Task_sleep(100);
    }

    /* All lamps off */
    SetLamp(0);
}

//*****************************************************************************
// Set default runtime values
//*****************************************************************************

void InitSysDefaults(SYSPARMS* p)
{
    /* default servo parameters */
    p->version                   = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    p->build                     = FIRMWARE_BUILD;
    p->debug                     = 0;           /* debug mode 0=off                 */

    p->sysflags					 = SF_BRAKES_STOP_PLAY | SF_ENGAGE_PINCH_ROLLER | SF_STOP_AT_TAPE_END;

    p->vel_detect_threshold      = 10;          /* 10 pulses or less = no velocity  */
    p->reel_offset_gain          = 0.150f;      /* reel torque null offset gain     */
    p->reel_radius_gain          = 1.000f;	    /* reeling radius gain              */
    p->tension_sensor_gain       = 0.07f;	    /* tension sensor arm gain          */

    p->debounce                  = 30;		    /* button debounce time             */
    p->lifter_settle_time        = 600;         /* tape lifter settling delay in ms */
    p->brake_settle_time         = 100;
    p->play_settle_time			 = 800;		    /* play after shuttle settle time   */
    p->pinch_settle_time         = 250;         /* start 250ms after pinch roller   */
    p->record_pulse_time     	 = REC_PULSE_TIME;
    p->rechold_settle_time    	 = REC_SETTLE_TIME;

    p->stop_supply_tension       = 360;         /* supply tension level (0-DAC_MAX) */
    p->stop_takeup_tension       = 385;         /* takeup tension level (0-DAC_MAX) */
    p->stop_brake_torque         = 650;    	    /* max dynamic stop brake torque   */

    p->shuttle_supply_tension    = 360;         /* shuttle supply reel tension      */
    p->shuttle_takeup_tension    = 385;         /* shuttle takeup reel tension      */
    p->shuttle_velocity          = 1000;        /* max shuttle velocity             */
    p->shuttle_lib_velocity      = 500;         /* max shuttle lib wind velocity    */
    p->shuttle_autoslow_velocity = 300;         /* reduce shuttle velocity speed to */
    p->autoslow_at_offset        = 65;          /* offset to trigger auto-slow      */
    p->autoslow_at_velocity      = 650;         /* reel speed to trigger auto-slow  */
    p->shuttle_fwd_holdback_gain = 0.010f;      /* hold back gain for rew shuttle   */
    p->shuttle_rew_holdback_gain = 0.015f;      /* hold back gain for fwd shuttle   */

    p->shuttle_servo_pgain       = PID_Kp;      /* shuttle mode servo P-gain        */
    p->shuttle_servo_igain       = PID_Ki;      /* shuttle mode servo I-gain        */
    p->shuttle_servo_dgain       = PID_Kd;      /* shuttle mode servo D-gain        */

    p->play_lo_takeup_tension    = 375;         /* takeup tension level             */
    p->play_lo_supply_tension    = 350;         /* supply tension level             */
    p->play_lo_boost_pgain       = 1.300f;      /* P-gain */
    p->play_lo_boost_igain       = 0.300f;      /* I-gain */
    p->play_lo_boost_end         = 25;          /* target play velocity */

    p->play_hi_takeup_tension    = 375;         /* takeup tension level             */
    p->play_hi_supply_tension    = 350;         /* supply tension level             */
    p->play_hi_boost_pgain       = 1.350f;      /* P-gain */
    p->play_hi_boost_igain       = 0.250f;      /* I-gain */
    p->play_hi_boost_end         = 118;         /* target play velocity */

    /* If running 1" tape width headstack, overwrite any members
     * that require different default values. Mainly we load the
     * tensions with 1/2 the values required for 2".
     */

    if (g_tape_width == 1)
    {
        p->stop_brake_torque         = 400;		/* max dynamic stop brake torque   */

        p->stop_supply_tension       /= 2;		/* supply tension level (0-DAC_MAX) */
        p->stop_takeup_tension       /= 2;		/* takeup tension level (0-DAC_MAX) */

        p->shuttle_supply_tension    /= 2;		/* shuttle supply reel tension      */
        p->shuttle_takeup_tension    /= 2;		/* shuttle takeup reel tension      */

        p->play_lo_takeup_tension    /= 2;		/* takeup tension level             */
        p->play_lo_supply_tension    /= 2;		/* supply tension level             */

        p->play_hi_takeup_tension    /= 2;		/* takeup tension level             */
        p->play_hi_supply_tension    /= 2;		/* supply tension level             */
    }
}

//*****************************************************************************
// Write system parameters from our global settings buffer to EEPROM.
//
// Returns:  0 = Sucess
//          -1 = Error writing EEPROM data
//*****************************************************************************

int32_t SysParamsWrite(SYSPARMS* sp)
{
    int32_t rc = 0;

    uint32_t uAddress;

    uAddress = (g_tape_width == 1) ? 0: sizeof(SYSPARMS);

    sp->version = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    sp->build   = FIRMWARE_BUILD;
    sp->magic   = MAGIC;

    rc = EEPROMProgram((uint32_t *)sp, uAddress, sizeof(SYSPARMS));

    System_printf("Writing System Parameters (size=%d)\n", sizeof(SYSPARMS));
    System_flush();

    return rc;
 }

//*****************************************************************************
// Read system parameters into our global settings buffer from EEPROM.
//
// Returns:  0 = Success
//          -1 = Error reading flash
//
//*****************************************************************************

int32_t SysParamsRead(SYSPARMS* sp)
{
    InitSysDefaults(sp);

    uint32_t uAddress = 0;

    uAddress = (g_tape_width == 1) ? 0: sizeof(SYSPARMS);

    EEPROMRead((uint32_t *)sp, uAddress, sizeof(SYSPARMS));

    if (sp->magic != MAGIC)
    {
        System_printf("ERROR Reading System Parameters - Resetting Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    if (sp->version != MAKEREV(FIRMWARE_VER, FIRMWARE_REV))
    {
        System_printf("WARNING New Firmware Version - Resetting Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    if (sp->build < FIRMWARE_MIN_BUILD)
    {
        System_printf("WARNING New Firmware BUILD - Resetting Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    System_printf("System Parameters Loaded (size=%d)\n", sizeof(SYSPARMS));
    System_flush();

    return 0;
}

/* End-Of-File */
