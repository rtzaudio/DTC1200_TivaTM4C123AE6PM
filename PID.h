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

// Constants
// Arbitrary numbers - pick appropriate values for
// the system of interest.
// INTERVAL = milliseconds for PV to react to CV
//#define PID_CVmax       255     	// 0 to 255
//#define PID_INTERVAL    120     	// 120ms between "I&D" updates

#define PID_Kp				0.56f
#define PID_Ki				0.19f
#define PID_Kd				0.01f

#define PID_TOLERANCE_F		3.0f	/* error tolerance */

/* Floating Point PID */

typedef struct _FPID {
    float   Kp;         	/* proportional gain */
    float   Ki;         	/* integral gain */
    float   Kd;         	/* derivative gain */
    float   error;          /* last setpoint error */
    float   esum;    		/* accumulated error */
    float	pvprev;         /* saved previous pv value */
    float	cvi;			/* integral component of CV */
    float	cvd;            /* derivative component of CV */
    float   cvmax;			/* maximum range for CV value */
    float	tolerance;		/* error tolerance */
} FPID;

// PID Function Prototypes

void fpid_init(FPID* p, float Kp, float Ki, float Kd, float cvmax, float tolerance);
float fpid_calc(FPID* p, float setpoint, float actual);

#endif /* __PID_H__ */

/* end-of-file */
