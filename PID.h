/*
 * <pid.h> - PID Algorithm Header
 */

#ifndef __PID_H__
#define __PID_H__

//#include "IQmathLib.h"

// Constants
// Arbitrary numbers - pick appropriate values for
// the system of interest.
// INTERVAL = milliseconds for PV to react to CV

//#define PID_CVmax       255     	// 0 to 255
//#define PID_INTERVAL    120     	// 120ms between "I&D" updates

// Gains
// Make these run-time tunable in a real system.
// The GAIN macro divides the gain values by 100
// to allow finer granularity using integer arithmetic.

//#define PID_Kp     		500         // Kp = 5.000
//#define PID_Ki     		10          // Ki = 0.100
//#define PID_Kd     		10000       // Kd = 100.0

//#define PID_Kp     		9         // Kp = 0.090
//#define PID_Ki     		15        // Ki = 0.1500
//#define PID_Kd     		3         // Kd = 0.03


#define PID_TOLERANCE	3  	/* error tolerance */

/* Integer PID */

typedef struct _IPID {
    long    Kp;         	/* proportional gain */
    long    Ki;         	/* integral gain */
    long    Kd;         	/* derivative gain */
    long    error;          /* last setpoint error */
    long    esum;    		/* accumulated error */
    long	pvprev;         /* saved previous pv value */
    long	cvi;			/* integral component of CV */
    long	cvd;            /* derivative component of CV */
    long 	tolerance;		/* error tolerance */
} IPID;

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
    float	tolerance;		/* error tolerance */
} FPID;

// PID Function Prototypes

void ipid_init(IPID* p, long Kp, long Ki, long Kd, long tolerance);
long ipid_calc(IPID* p, long setpoint, long actual);

void fpid_init(IPID* p, float Kp, float Ki, float Kd, float tolerance);
float fpid_calc(IPID* p, float setpoint, float actual);

#endif /* __PID_H__ */

/* end-of-file */
