/* ============================================================================
 *
 * STC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================ */

#ifndef _IPCMESSAGE_H_
#define _IPCMESSAGE_H_

/* ============================================================================
 * Message class types for IPCMSG.type
 * ============================================================================ */

#define IPC_TYPE_NOTIFY				10      /* Notifications from DTC to STC  */
#define IPC_TYPE_CONFIG		        20      /* DTC config Get/Set transaction */
#define IPC_TYPE_TRANSPORT          30      /* DTC transport control commands */

/* IPC_TYPE_NOTIFY Operation codes to DTC from STC */
#define OP_NOTIFY_BUTTON			100
#define OP_NOTIFY_TRANSPORT			101
#define OP_NOTIFY_EOT               102
#define OP_NOTIFY_LAMP              103

/* IPC_TYPE_CONFIG Operation codes from STC to DTC */
#define OP_GET_SHUTTLE_VELOCITY     200
#define OP_SET_SHUTTLE_VELOCITY     201

/* IPC_TYPE_TRANSPORT Operation codes STC->DTC */
#define OP_MODE_STOP                300
#define OP_MODE_PLAY                301
#define OP_MODE_FWD                 302     /* param1 specifies velocity */
#define OP_MODE_FWD_LIB             303
#define OP_MODE_REW                 304     /* param1 specifies velocity */
#define OP_MODE_REW_LIB             305

#define OP_TRANSPORT_GET_MODE       320     /* get current transport mode */
#define OP_TRANSPORT_GET_VELOCITY   321
#define OP_TRANSPORT_GET_TACH       322

/* ============================================================================
 * Notification Bit Flags
 * ============================================================================ */

#if 0
/* OP_NOTIFY_BUTTON bits for param1.U */
#define S_STOP          0x01                /* stop button       */
#define S_PLAY          0x02                /* play button        */
#define S_REC           0x04                /* record button      */
#define S_REW           0x08                /* rewind button      */
#define S_FWD           0x10                /* fast fwd button    */
#define S_LDEF          0x20                /* lift defeat button */
#define S_TAPEOUT       0x40                /* tape detect out    */
#define S_TAPEIN        0x80                /* tape detect in     */
#endif

#endif /* _IPCMESSAGE_H_ */
