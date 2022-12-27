/*
  * DTC-1200 Digital Channel Switcher Command Messages
 *
 * Copyright (C) 2021, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * InterProcess Communications (IPC) Services via serial link.
 *
 */

#ifndef _IPCCMD_DTC1200_H_
#define _IPCCMD_DTC1200_H_

/***************************************************************************/
/*** DTC IPC MESSAGE HEADER ************************************************/
/***************************************************************************/

#ifndef _DTC_CONFIG_DATA_DEFINED_
#define _DTC_CONFIG_DATA_DEFINED_

/* Configuration Parameters - MUST MATCH SYSPARMS STRUCT IN DTC1200.h */
typedef struct _DTC_CONFIG_DATA {
    uint32_t magic;
    uint32_t version;
    uint32_t build;
    /*** GLOBAL PARAMETERS ***/
    int32_t debug;                      /* debug level */
    int32_t pinch_settle_time;          /* delay before engaging play mode   */
    int32_t lifter_settle_time;         /* lifter settling time in ms        */
    int32_t brake_settle_time;          /* break settling time after STOP    */
    int32_t play_settle_time;           /* play after shuttle settling time  */
    int32_t rechold_settle_time;        /* record pulse length time          */
    int32_t record_pulse_time;          /* record pulse length time          */
    int32_t vel_detect_threshold;       /* vel detect threshold (10)         */
    uint32_t debounce;                  /* debounce transport buttons time   */
    uint32_t sysflags;                  /* global system bit flags           */
    /*** SOFTWARE GAIN PARAMETERS ***/
    float   reel_radius_gain;           /* reeling radius play gain factor   */
    float   reel_offset_gain;           /* reeling radius offset gain factor */
    float   tension_sensor_gain;        /* tension sensor gain divisor       */
    float   tension_sensor_midscale1;   /* ADC mid-scale for 1" tape         */
    float   tension_sensor_midscale2;   /* ADC mid-scale for 2" tape         */
    /*** THREAD TAPE PARAMETERS ***/
    int32_t thread_supply_tension;      /* supply tension level (0-DAC_MAX)  */
    int32_t thread_takeup_tension;      /* takeup tension level (0-DAC_MAX)  */
    /*** STOP SERVO PARAMETERS ***/
    int32_t stop_supply_tension;        /* supply tension level (0-DAC_MAX)  */
    int32_t stop_takeup_tension;        /* takeup tension level (0-DAC_MAX)  */
    int32_t stop_brake_torque;          /* stop brake torque in shuttle mode */
    /*** SHUTTLE SERVO PARAMETERS ***/
    int32_t shuttle_supply_tension;     /* play supply tension (0-DAC_MAX)   */
    int32_t shuttle_takeup_tension;     /* play takeup tension               */
    int32_t shuttle_velocity;           /* target speed for shuttle mode     */
    int32_t shuttle_lib_velocity;       /* library wind mode velocity        */
    int32_t shuttle_autoslow_velocity;  /* velocity to reduce speed to       */
    int32_t autoslow_at_offset;         /* auto-slow trigger at offset       */
    int32_t autoslow_at_velocity;       /* auto-slow trigger at velocity     */
    float   shuttle_fwd_holdback_gain;  /* velocity tension gain factor      */
    float   shuttle_rew_holdback_gain;  /* velocity tension gain factor      */
    /* reel servo PID values */
    float   shuttle_servo_pgain;        /* P-gain */
    float   shuttle_servo_igain;        /* I-gain */
    float   shuttle_servo_dgain;        /* D-gain */
    /*** PLAY SERVO PARAMETERS ***/
    /* play high speed boost parameters */
    int32_t play_hi_supply_tension;     /* play supply tension (0-DAC_MAX) */
    int32_t play_hi_takeup_tension;     /* play takeup tension (0-DAC_MAX) */
    int32_t play_hi_boost_end;
    float   play_hi_boost_pgain;        /* P-gain */
    float   play_hi_boost_igain;        /* I-gain */
    /* play low speed boost parameters */
    int32_t play_lo_supply_tension;     /* play supply tension (0-DAC_MAX) */
    int32_t play_lo_takeup_tension;     /* play takeup tension (0-DAC_MAX) */
    int32_t play_lo_boost_end;
    float   play_lo_boost_pgain;        /* P-gain */
    float   play_lo_boost_igain;        /* I-gain */
} DTC_CONFIG_DATA;

/* System Bit Flags for DTC1200_CONFIG.sysflags */
#define DTC_SF_LIFTER_AT_STOP       0x0001  /* leave lifter engaged at stop */
#define DTC_SF_BRAKES_AT_STOP       0x0002  /* leave brakes engaged at stop */
#define DTC_SF_BRAKES_STOP_PLAY     0x0004  /* use brakes to stop play mode */
#define DTC_SF_ENGAGE_PINCH_ROLLER  0x0008  /* engage pinch roller at play  */
#define DTC_SF_STOP_AT_TAPE_END     0x0010  /* stop @tape end leader detect */

#endif /*_DTC_CONFIG_DATA_DEFINED_*/

/***************************************************************************/
/*** IPC MESSAGE OP-CODE TYPES *********************************************/
/***************************************************************************/

/* Command Codes for DTC_IPCMSG_HDR.opcode */
#define DTC_OP_CONFIG_EPROM     100         /* store/recall config eprom   */
#define DTC_OP_CONFIG_GET       101         /* get configuration data      */
#define DTC_OP_CONFIG_SET       102         /* set configuration data      */
#define DTC_OP_TRANSPORT_CMD    200         /* transport command requests  */

/***************************************************************************/
/*** IPC MESSAGE DATA STRUCTURES *******************************************/
/***************************************************************************/

/* Define the default message header structure */
typedef IPCMSG_HDR DTC_IPCMSG_HDR;

/*** STORE/RECALL CONFIG FROM EPOM *****************************************/

typedef struct _DTC_IPCMSG_CONFIG_EPROM {
    DTC_IPCMSG_HDR  hdr;
    int32_t         store;                  /* 0=recall, 1=store to EPROM */
    int32_t         status;
} DTC_IPCMSG_CONFIG_EPROM;

/*** GET CONFIG DATA *******************************************************/

typedef struct _DTC_IPCMSG_CONFIG_GET {
    DTC_IPCMSG_HDR  hdr;
    DTC_CONFIG_DATA cfg;                    /* global config data returned */
} DTC_IPCMSG_CONFIG_GET;

/*** SET CONFIG DATA *******************************************************/

typedef struct _DTC_IPCMSG_CONFIG_SET {
    DTC_IPCMSG_HDR  hdr;
    DTC_CONFIG_DATA cfg;                    /* global config data to store */
} DTC_IPCMSG_CONFIG_SET;

/*** TRANSPORT COMMAND *****************************************************/

typedef struct _DTC_IPCMSG_TRANSPORT_CMD {
    DTC_IPCMSG_HDR  hdr;
    int32_t         cmd;                    /* transport command requested */
    uint16_t        param1;                 /* parameter flags */
    uint16_t        param2;                 /* parameter flags */
} DTC_IPCMSG_TRANSPORT_CMD;

/* Transport command modes */
typedef enum DTCTransportCommand {
    DTC_Transport_STOP,                     /* transport stop mode */
    DTC_Transport_PLAY,                     /* transport play mode */
    DTC_Transport_FWD,                      /* shuttle forward mode */
    DTC_Transport_FWD_LIB,                  /* shuttle forward lib wind mode */
    DTC_Transport_REW,                      /* shuttle rewind mode */
    DTC_Transport_REW_LIB                   /* shuttle rewind lib wind mode */
} DTCTransportCommand;

#endif /* _IPCCMD_DTC1200_H_ */
