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
#include "TransportTask.h"
#include "ServoTask.h"
#include "TerminalTask.h"
#include "ServoTask.h"
#include "TapeTach.h"
#include "MotorDAC.h"
#include "IOExpander.h"
#include "Diag.h"
#include "IPCServer.h"

/* Global Data Items */

Semaphore_Handle g_semaSPI;
Semaphore_Handle g_semaServo;
Semaphore_Handle g_semaTransportMode;

extern IOExpander_Handle g_handleSPI1;
extern IOExpander_Handle g_handleSPI2;

#if BUTTON_INTERRUPTS != 0
Event_Handle g_eventSPI;
#endif

/* Static Function Prototypes */

Int main();
Void MainControlTask(UArg a0, UArg a1);

static bool ReadSerialNumber(I2C_Handle handle, uint8_t ui8SerialNumber[16]);

static void FlashLEDSuccess(void);
static void FlashLEDError(void);

//*****************************************************************************
// Main Program Entry Point
//*****************************************************************************

Int main()
{
    Error_Block eb;
    Mailbox_Params mboxParams;
    Task_Params taskParams;

    /* Enables Floating Point Hardware Unit */
    FPUEnable();
    /* Allows the FPU to be used inside interrupt service routines */
    //FPULazyStackingEnable();

    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initUART();
    Board_initI2C();
    Board_initSPI();
    Board_initADC();

    /* Create transport controller task mailbox */

    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxCommander = Mailbox_create(sizeof(uint8_t), 8, &mboxParams, &eb);

    /* Create servo controller task mailbox */

    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxController = Mailbox_create(sizeof(CMDMSG), 8, &mboxParams, &eb);

    /* Create a global binary semaphores for serialized access items */

    Error_init(&eb);
    g_semaSPI = Semaphore_create(1, NULL, &eb);

    Error_init(&eb);
    g_semaServo = Semaphore_create(1, NULL, &eb);

    Error_init(&eb);
    g_semaTransportMode = Semaphore_create(1, NULL, &eb);

    /* Now start the main application button polling task */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1248;
    taskParams.priority  = 13;

    if (Task_create(MainControlTask, &taskParams, &eb) == NULL)
        System_abort("MainControlTask!\n");

    BIOS_start();    /* does not return */

    return(0);
}

//*****************************************************************************
// Initialize and open various system peripherals we'll be using
//*****************************************************************************

void InitPeripherals(void)
{
    I2C_Params i2cParams;

    /*
     * Open the I2C ports for peripherals we need to communicate with.
     */

    /* Open I2C0 for U14 CAT24C08 EPROM */
    I2C_Params_init(&i2cParams);
    i2cParams.transferMode  = I2C_MODE_BLOCKING;
    i2cParams.bitRate       = I2C_100kHz;
    g_handleI2C0 = I2C_open(Board_I2C0, &i2cParams);

    /* Open I2C1 for U9 AT24CS01 EPROM/Serial# */
    I2C_Params_init(&i2cParams);
    i2cParams.transferMode  = I2C_MODE_BLOCKING;
    i2cParams.bitRate       = I2C_100kHz;
    g_handleI2C1 = I2C_open(Board_I2C1, &i2cParams);

    /*
     * Open the SPI ports for peripherals we need to communicate with.
     */

    /* Initialize the SPI-0 to the reel motor DAC's */
    MotorDAC_initialize();

    /* Initialize the SPI-1 & SPI-2 to the I/O expanders */
    IOExpander_initialize();

    /* Read the serial number into memory */
    ReadSerialNumber(g_handleI2C1, g_ui8SerialNumber);
}

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
// The main application initialization, setup and button controler task.
//*****************************************************************************

#define DEBOUNCE	6

Void MainControlTask(UArg a0, UArg a1)
{
    uint8_t count = 0;
    uint8_t bits = 0x00;
    uint8_t temp;
    uint8_t tran_prev = 0xff;
    uint8_t mode_prev = 0xff;
    uint8_t tout_prev = 0xff;
    uint8_t debounce_tran = 0;
    uint8_t debounce_mode = 0;
    uint8_t debounce_tout = 0;
    uint8_t eot_count = 0;
    uint8_t eot_state = 0;
    int status = 0;
    IPCMSG ipc;
    Error_Block eb;
    Task_Params taskParams;

    /* Initialize the default servo and program data values */

    memset(&g_servo, 0, sizeof(SERVODATA));
    memset(&g_sys, 0, sizeof(SYSPARMS));

    InitPeripherals();

    /* Detect tape width from head stack mounted */
    g_tape_width = (GPIO_read(Board_TAPE_WIDTH) == 0) ? 2 : 1;

    /* Read the initial mode switch states */
    GetModeSwitches(&mode_prev);
    g_high_speed_flag = (mode_prev & M_HISPEED) ? 1 : 0;
    g_dip_switch      = (mode_prev & M_DIPSW_MASK);

    /* Read the initial transport switch states */
    GetTransportSwitches(&tout_prev);
    g_tape_out_flag   = (tout_prev & S_TAPEOUT) ? 1 : 0;

    /* Initialize the default system parameters */
    InitSysDefaults(&g_sys);

    /* Initialize the IPC interface to STC-1200 card */
    IPC_Server_init();

    /* Read the system config parameters from storage.
     * If DIP switch 4 is set we only run with defaults
     * and ignore the settings in flash.
     */
    if (!(g_dip_switch & M_DIPSW4))
    {
        status = SysParamsRead(&g_sys);
    }

    /* Start up the various system task threads */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 9;
    if (Task_create(TransportCommandTask, &taskParams, &eb) == NULL)
        System_abort("TransportCommandTask()!n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 10;
    if (Task_create(TransportControllerTask, &taskParams, &eb) == NULL)
        System_abort("TransportControllerTask()!n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 2048;
    taskParams.priority  = 1;
    if (Task_create(TerminalTask, &taskParams, &eb) == NULL)
        System_abort("TerminalTask()!\n");

    /* Start the transport servo loop timer clock running.
     * We create an auto-reload periodic timer for the servo loop.
     * Note we are servicing the servo task at 500 Hz.
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 15;

    if (Task_create(ServoLoopTask, &taskParams, &eb) == NULL)
        System_abort("Servo loop task create failed!\n");

    /****************************************************************
     * Enter the main application button processing loop forever.
     ****************************************************************/

    /* Startup the IPC server threads */
    IPC_Server_startup();

    /* Send initial tape arm out state to transport ctrl/cmd task */
    temp = (g_tape_out_flag) ? S_TAPEOUT : S_TAPEIN;
    Mailbox_post(g_mailboxCommander, &temp, 10);

    /* Blink all lamps if error reading EPROM config data, otherwise
     * blink each lamp sequentially on success.
     */

    if (status != 0)
        FlashLEDError();
    else
        FlashLEDSuccess();

    /* Set initial status blink LED mask */
    g_lamp_blink_mask = L_STAT1;

    for(;;)
    {
        /* Blink heartbeat LED1 on the transport interface card */
        if (++count >= 125)
        {
            count = 0;
            g_lamp_mask ^= g_lamp_blink_mask;
        }

        /* Set any new led/lamp state. We only update the LED output
         * port if a new lamp state was selected.
         */

        if (g_lamp_mask != g_lamp_mask_prev)
        {
            /* Set the new lamp state */
            SetLamp(g_lamp_mask);

            /* Don't send status 1-3 LED notifications as the STC doesn't
             * need this and it creates lots of unneeded IPC traffic.
             */
            if ((g_lamp_mask_prev & L_LAMP_MASK) != (g_lamp_mask & L_LAMP_MASK))
            {
                /* Notify STC of the lamp mask change & current transport mode */
                ipc.type     = IPC_TYPE_NOTIFY;
                ipc.opcode   = OP_NOTIFY_LAMP;
                ipc.param1.U = (uint32_t)g_lamp_mask;
                ipc.param2.U = (g_high_speed_flag != 0) ? 30 : 15;

                IPC_Notify(&ipc, 0);
            }

            // Update the previous lamp state
            g_lamp_mask_prev = g_lamp_mask;
        }

        /* Notify the STC of any transport mode changes */

        if (g_notify_mode != g_notify_mode_prev)
        {
            g_notify_mode_prev = g_notify_mode;

            /* Notify the STC of the new transport mode */
            ipc.type     = IPC_TYPE_NOTIFY;
            ipc.opcode   = OP_NOTIFY_TRANSPORT;
            ipc.param1.U = g_notify_mode;
            ipc.param2.U = (g_high_speed_flag) ? 30 : 15;

            IPC_Notify(&ipc, 0);
        }

        /* Poll the transport switches to see if the tape out arm on the
         * right hand side of the machine is triggered. If not then we
         * continue processing polling for any transport buttons pressed.
         */

        GetTransportSwitches(&bits);

        /* First process the tape out arm switch */
        if ((bits & S_TAPEOUT) != tout_prev)
        {
            if (++debounce_tout >= DEBOUNCE)
            {
                debounce_tout = 0;

                /* Save the new state */
                tout_prev = (bits & S_TAPEOUT);

                /* Set the tape out arm state */
                g_tape_out_flag = (bits & S_TAPEOUT) ? 1 : 0;

                if (!(bits & S_TAPEOUT))
                    bits |= S_TAPEIN;

                /* Send the button press to transport ctrl/cmd task */
                Mailbox_post(g_mailboxCommander, &bits, 10);
            }
        }

        /* Next process the tape transport buttons */

        bits &= S_BUTTON_MASK;

        if (bits)
        {
        	temp = bits & ~(S_REC);

			if (temp != tran_prev)
			{
				if (++debounce_tran >= DEBOUNCE)
				{
					debounce_tran = 0;

					/* Debounced a button press, send it to transport task */
					tran_prev = temp;

					/* Send the button press to transport ctrl/cmd task */
					Mailbox_post(g_mailboxCommander, &bits, 10);

					/* Let STC know button status change */
					ipc.type     = IPC_TYPE_NOTIFY;
					ipc.opcode   = OP_NOTIFY_BUTTON;
					ipc.param1.U = bits;
					ipc.param2.U = (g_high_speed_flag != 0) ? 30 : 15;

					IPC_Notify(&ipc, 0);
				}
			}
        }
        else
        {
        	tran_prev = 0xff;
        }

        /* Poll the mode config switches to read the DIP switches on
         * the PCB and the hi/lo speed switch on the transport control.
         */

        GetModeSwitches(&bits);

        if (bits != mode_prev)
        {
            if (++debounce_mode >= DEBOUNCE)
            {
                debounce_mode = 0;

                /* We debounced a switch change, reset and process it */
                mode_prev = bits;

                /* Save the transport speed select setting */
                g_high_speed_flag = (bits & M_HISPEED) ? 1 : 0;

                /* Save the updated DIP switch settings */
                g_dip_switch = bits & M_DIPSW_MASK;

                g_lamp_mask_prev = 0xFF;
            }
        }

        /* Poll the accessory optical tape end sensor. If the sensor
         * is active, we issue a STOP command to halt the transport.
         */

        switch(eot_state)
        {
        case 0:
            /* Look for first high edge */
            if ((bits = GPIO_read(Board_TAPE_END)) > 0)
                eot_state = 1;
            eot_count = 0;
            break;

        case 1:
            /* If it goes low before 25 counts, then it's a glitch */
            if ((bits = GPIO_read(Board_TAPE_END)) == 0)
            {
                eot_state = 0;
                break;
            }
            /* Trigger signal been high for at least 25 samples */
            if (++eot_count > 25)
                eot_state = 2;
            break;

        case 2:
            /* Send a STOP button press to transport ctrl/cmd task */
            bits = S_STOP;
            Mailbox_post(g_mailboxCommander, &bits, 10);

            /* Let STC know we're at end of tape (EOT) */
            ipc.type     = IPC_TYPE_NOTIFY;
            ipc.opcode   = OP_NOTIFY_EOT;
            ipc.param1.U = bits;
            ipc.param2.U = 0;
            IPC_Notify(&ipc, 0);

            eot_state = 3;
            break;

        case 3:
            /* Now wait for trigger to go back low on release */
            if ((bits = GPIO_read(Board_TAPE_END)) == 0)
                eot_state = 0;
            break;
        }

        /* delay for 10ms and loop */
        Task_sleep(5);
    }
}

//*****************************************************************************
// Set default runtime values
//*****************************************************************************

void InitSysDefaults(SYSPARMS* p)
{
    /* default servo parameters */
    p->version                   = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    p->debug                     = 0;           /* debug mode 0=off                 */

    p->sysflags					 = SF_BRAKES_STOP_PLAY | SF_ENGAGE_PINCH_ROLLER;

    p->vel_detect_threshold      = 5;           /* 10 pulses or less = no velocity  */
    p->reel_offset_gain          = 0.100f;      /* reel torque null offset gain     */
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
    p->stop_brake_torque         = 575;    	    /* max dynamic stop brake torque   */

    p->shuttle_supply_tension    = 360;         /* shuttle supply reel tension      */
    p->shuttle_takeup_tension    = 385;         /* shuttle takeup reel tension      */
    p->shuttle_velocity          = 500;         /* max shuttle velocity             */
    p->shuttle_lib_velocity      = 250;         /* max shuttle lib wind velocity    */
    p->shuttle_autoslow_offset   = 40;          /* offset to reduce velocity at     */
    p->shuttle_autoslow_velocity = 0;           /* reduce shuttle velocity speed to */
    p->shuttle_holdback_gain     = 0.050f;      /* hold back gain during shuttle    */
    p->shuttle_servo_pgain       = PID_Kp;      /* shuttle mode servo P-gain        */
    p->shuttle_servo_igain       = PID_Ki;      /* shuttle mode servo I-gain        */
    p->shuttle_servo_dgain       = PID_Kd;      /* shuttle mode servo D-gain        */

    p->play_lo_takeup_tension    = 375;         /* takeup tension level             */
    p->play_lo_supply_tension    = 350;         /* supply tension level             */
    p->play_lo_boost_pgain       = 1.300f;      /* P-gain */
    p->play_lo_boost_igain       = 0.300f;      /* I-gain */
    p->play_lo_boost_end         = 28;          /* target play velocity */

    p->play_hi_takeup_tension    = 375;         /* takeup tension level             */
    p->play_hi_supply_tension    = 350;         /* supply tension level             */
    p->play_hi_boost_pgain       = 1.350f;      /* P-gain */
    p->play_hi_boost_igain       = 0.250f;      /* I-gain */
    p->play_hi_boost_end         = 118;         /* target play velocity */

    p->reserved3                 = 0;           /* reserved */
    p->reserved4                 = 0;           /* reserved */

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
        System_printf("ERROR Reading System Parameters - Using Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    if (sp->version != MAKEREV(FIRMWARE_VER, FIRMWARE_REV))
    {
        System_printf("WARNING New Firmware Version - Using Defaults...\n");
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
