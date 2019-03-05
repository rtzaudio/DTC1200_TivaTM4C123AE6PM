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
#include "DTC1200_TivaTM4C123AE6PMI.h"
#include "pid.h"

/*******************************************************************************
 * FLOATING POINT PID FUNCTIONS
 ******************************************************************************/

/*
 * Function:    fpid_init()
 *
 * Synopsis:    void fpid_init(p, Kp, Ki, Kd, tolerance)
 *
 *              PID* p;         -    Pointer to PID data structure.
 *              float Kp;           - Proportional gain
 *              float Ki;           - Integral gain
 *              float Kd;           - Derivative gain
 *              float cvmax;        - Maximum CV value allowed
 *              float tolerance;    - Error tolerance
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
    float   cvmax,
    float   cvmin,
    float   tolerance
    )
{
    /* maximum CV range allowed (eg, DAC max) */
    p->iMax = cvmax;
    p->iMin = cvmin;

    /* dead-band error tolerance we'll allow */
    p->tolerance = tolerance;

    /* initialize PID variables */
    p->Kp = Kp;     /* proportional gain */
    p->Ki = Ki;     /* integral gain */
    p->Kd = Kd;     /* derivative gain */

    /* zero out accumulators */
    p->error  = 0.0f;

    p->iState = 0.0f;
    p->dState = 0.0f;
}

/*
 * Function:    fpid_calc()
 *
 * Synopsis:    float fpid_velocity_calc(p, setpoint, actual)
 *
 *              PID* p;         - Pointer to PID data structure.
 *              float setpoint; - Desired setpoint value.
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
    float cv;
    float pTerm;
    float dTerm;
    float iTerm;

    /* Calculate the setpoint error */
    p->error = setpoint - actual;

    /* Error is within dead band tolerance? */
    //if (fabsf(p->error) < p->tolerance)
    //    p->error = 0.0f;

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
#if 1
    dTerm = p->Kd * (setpoint - p->dState);
    p->dState = setpoint;
#else
    dTerm = p->Kd * (p->dState - actual);
    p->dState = actual;
#endif

    /* Calculate the final CV term */
    cv = pTerm + iTerm - dTerm;

    /* Clamp CV to allowed range if necessary */
    if (cv < p->iMin)
        cv = p->iMin;
    else if (cv > p->iMax)
        cv = p->iMax;

    return cv;
}

/* End-Of-File */
