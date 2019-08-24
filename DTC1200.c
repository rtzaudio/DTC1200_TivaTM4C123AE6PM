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
#include "Utils.h"

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

    /* Initialize the SPI-1 & SPI-2 to the I/O expanders */
    IOExpander_initialize();

    /* Initialize the SPI-0 to the reel motor DAC's */
    MotorDAC_initialize();

    /* Read the serial number into memory */
    ReadSerialNumber(g_handleI2C1, g_ui8SerialNumber);
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
    IPC_MSG ipc;
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
     * If DIP switch 4 is set, then we run with defaults
     * instead and ignore the settings in flash.
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

        if (g_sys.sysflags & SF_STOP_AT_TAPE_END)
        {
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
                /* Ignore EOT sensor if tape out arm is open */
                if (g_tape_out_flag)
                    break;

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
        }

        /* delay for 5ms and loop */
        Task_sleep(5);
    }
}

/* End-Of-File */
