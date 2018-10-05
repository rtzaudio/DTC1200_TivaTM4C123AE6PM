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

#include <limits.h>
#include <stdint.h>
#include <math.h>
#include "pid.h"
#include "DTC1200_TivaTM4C123AE6PMI.h"

/*******************************************************************************
 * FLOATING POINT PID FUNCTIONS
 ******************************************************************************/

#if (PID_TYPE == 0)

/*
 * Function:    fpid_init()
 *
 * Synopsis:    void fpid_init(p, Kp, Ki, Kd, tolerance)
 *
 *              PID* p;     	-	 Pointer to PID data structure.
 *              float Kp;			- Proportional gain
 *              float Ki;    		- Integral gain
 *              float Kd;			- Derivative gain
 *              float cvmax;		- Maximum CV value allowed
 *              float tolerance;	- Error tolerance
 *
 * Description: This function initializes the static members of the PID
 *              data structure to the initial starting state.
 *
 * Returns:     void
 */

void fpid_init(
    FPID*   p,
    float   Kp,
	float   Ki,
	float   Kd,
	float	cvmax,
	float   tolerance
    )
{
	/* maximum CV range allowed (eg, DAC max) */
    p->cvmax = cvmax;

    /* dead-band error tolerance we'll allow */
    p->tolerance = tolerance;

    /* initialize PID variables */
    p->Kp     = Kp;			/* proportional gain */
    p->Ki     = Ki;			/* integral gain */
    p->Kd     = Kd;			/* derivative gain */

    /* zero out accumulators */
    p->error  = 0.0f;
    p->esum   = 0.0f;
    p->pvprev = 0.0f;
    p->cvi    = 0.0f;
    p->cvd    = 0.0f;
}

/*
 * Function:    fpid_calc()
 *
 * Synopsis:    float fpid_velocity_calc(p, setpoint, actual)
 *
 *              PID* p;         - Pointer to PID data structure.
 *              float setpoint;	- Desired setpoint value.
 *              float actual;   - Actual measured value from sensor.
 *
 * Description: This routine is designed to be called at fixed (or near fixed)
 *              time intervals either under a timer interrupt or from a foreground
 *              polling loop to perform PID calculations.
 *
 *              A digital controller measures the controlled variable at specific
 *              times, which are separated by a time interval called the sampling
 *              time, Delta T. The controller subtracts each sample of the measured
 *              variable from the setpoint to determine a set of error samples.
 *
 * Returns:     The PID output control variable (CV) value.
 */


float fpid_calc(FPID* p, float setpoint, float actual)
{
	float cv;	// return value

	/* Calculate the setpoint error */
	p->error = setpoint - actual;

	/* Error is within tolerance. */
	if (fabsf(p->error) < p->tolerance)
		p->error = 0.0f;

	/* Compute proportional term
	 * Kp * E
	 */
	cv = p->Kp * p->error;

	/* Compute integral term by summing errors. */
	p->esum = p->esum + p->error;

	/* Limit the proportional term to CV range */
	if (p->esum > p->cvmax)
		p->esum = p->cvmax;
	else if (p->esum < 0.0f)
		p->esum = 0.0f;

	/* Limit integral term to CV range. */
	//float Ki = (float)DAC_MAX / p->Ki;

	float Ki = p->Ki;
	if (Ki > p->cvmax)
		Ki = p->cvmax;

	/* calculate the integral term
	 * Ki * Esum;
	 */
	p->cvi = Ki * p->esum;

	/* calculate the derivative term
	 * Kd * PVdelta
	 */
	p->cvd = p->Kd * (p->pvprev - actual);
	p->pvprev = actual;

	/* Add terms: P+I+D */
	cv = cv + p->cvi + p->cvd;

	/* Clamp CV to allowed range if necessary */
	if (cv < 0.0f)
		cv = 0.0f;
	else if (cv > p->cvmax)
		cv = p->cvmax;

	return cv;
}

#else

void fpid_init(
    FPID*   p,
    float   Kp,
    float   Ki,
    float   Kd,
    float   cvmax,
    float   tolerance
    )
{
    /* maximum CV range allowed (eg, DAC max) */
    p->iMax = cvmax;
    p->iMin = 0.0f;

    /* dead-band error tolerance we'll allow */
    //p->tolerance = tolerance;

    /* initialize PID variables */
    p->Kp = Kp;     /* proportional gain */
    p->Ki = Ki;     /* integral gain */
    p->Kd = Kd;     /* derivative gain */

    /* zero out accumulators */
    p->error  = 0.0f;

    p->iState = 0.0f;
    p->dState = 0.0f;
}


float fpid_calc(FPID* p, float setpoint, float actual)
{
    float pTerm;
    float dTerm;
    float iTerm;

    /* Calculate the setpoint error */
    p->error = setpoint - actual;

    /* Error is within dead band tolerance? */
    //if (fabsf(pid->error) < PID_TOLERANCE_F)
    //    pid->error = 0.0f;

    /* Calculate the proportional term */
    pTerm = p->Kp * p->error;

    /* Calculate the integral state with appropriate limiting */
    p->iState += p->error;

    if (p->iState > p->iMax)
        p->iState = p->iMax;
    else if (p->iState < p->iMin)
        p->iState = p->iMin;

    /* Calculate the integral term */
    iTerm = p->Ki * p->iState;

    /* Calculate the derivative term */
    dTerm = p->Kd * (setpoint - p->dState);

    p->dState = setpoint;

    return pTerm + iTerm - dTerm;
}

#endif

/* End-Of-File */
