/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================ */

#ifndef __PID_H__
#define __PID_H__

/* Default PID Constants */
#define PID_Kp				3.000f
#define PID_Ki				0.300f
#define PID_Kd				0.025f

#define PID_TOLERANCE_F		3.0f	/* error tolerance */

/* Floating Point PID */
typedef struct _FPID {
    float       error;          /* current error state  */
    float       dState;         /* Last position input  */
    float       iState;         /* Integrator state     */
    float       iMax;           /* max integrator state */
    float       iMin;           /* min integrator state */
    /* PID gain values */
    float       Kp;             /* Proportional gain    */
    float       Ki;             /* Integral gain        */
    float       Kd;             /* Derivative gain      */
} FPID;

/* PID Function Prototypes */

void fpid_init(FPID* p, float Kp, float Ki, float Kd, float cvmax, float tolerance);
float fpid_calc(FPID* p, float setpoint, float actual);

#endif /* __PID_H__ */

/* end-of-file */
