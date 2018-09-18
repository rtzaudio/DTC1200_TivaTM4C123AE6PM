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

#ifndef DTC1200_TIVATM4C123AE6PMI_TRANSPORTTASK_H_
#define DTC1200_TIVATM4C123AE6PMI_TRANSPORTTASK_H_

/* Transport Message Command Structure */
typedef struct _CMDMSG {
    uint8_t     command;        /* command code   */
    uint8_t     opcode;         /* operation code */
    uint16_t    param1;         /* 16-bit param   */
} CMDMSG;

/* Transport Control Command Codes */
#define CMD_TRANSPORT_MODE		1		/* set the current transport mode */
#define CMD_STROBE_RECORD		2		/* op=1 punch-in, op=0 punch out */
#define CMD_TOGGLE_LIFTER		3		/* toggle tape lifter state */

/* Transport Controller Function Prototypes */

Void TransportCommandTask(UArg a0, UArg a1);
void TransportControllerTask(UArg a0, UArg a1);

Bool QueueTransportCommand(uint8_t command, uint8_t opcode, uint16_t param1);

#endif /* DTC1200_TIVATM4C123AE6PMI_TRANSPORTTASK_H_ */
