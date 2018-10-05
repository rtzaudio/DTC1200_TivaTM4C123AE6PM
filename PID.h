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

#define PID_TYPE    1

// Default PID Constants
#define PID_Kp				1.500f
#define PID_Ki				0.175f
#define PID_Kd				0.025f

#define PID_TOLERANCE_F		3.0f	/* error tolerance */

/* Floating Point PID */
#if (PID_TYPE == 0)
typedef struct _FPID {
    float       Kp;         	/* proportional gain */
    float       Ki;         	/* integral gain */
    float       Kd;         	/* derivative gain */
    float       error;          /* last setpoint error */
    float       esum;    		/* accumulated error */
    float	    pvprev;         /* saved previous pv value */
    float	    cvi;			/* integral component of CV */
    float	    cvd;            /* derivative component of CV */
    float       cvmax;			/* maximum range for CV value */
    float	    tolerance;		/* error tolerance */
} FPID;

#else

typedef struct _FPID {
    float       error;
    float       dState;         /* Last position input */
    float       iState;         /* Integrator state    */
    // Max/Min allowable integrator state
    float       iMax;
    float       iMin;
    // PDI gain values
    float       Kp;             /* proportional gain */
    float       Ki;             /* integral gain */
    float       Kd;             /* derivative gain */
} FPID;
#endif

// PID Function Prototypes

void fpid_init(FPID* p, float Kp, float Ki, float Kd, float cvmax, float tolerance);
float fpid_calc(FPID* p, float setpoint, float actual);

#endif /* __PID_H__ */

/* end-of-file */
