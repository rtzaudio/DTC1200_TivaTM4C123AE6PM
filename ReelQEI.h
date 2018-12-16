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

/* Tape moves at 30 IPS in high speed and 15 IPS in low speed. We want to
 * sample and correct at 0.3" intervals so our sample rate needs to
 * be 100 Hz (30/0.3 = 100). The timer delay period required in
 * milliseconds is 1/Hz * 1000. Therefore 1/100*1000 = 10ms. Therefore,
 * if we to correct at 0.15" intervals, the sample period would be 5ms.
 */

/* This macro is used to calculate the RPM of the motor based on velocity.
 * We are using Austria Micro Systems AS5047D with 500 pulses per revolution.
 * The QEI module is configured to capture both quadrature edges. Thus,
 * the 500 PPR encoder at four edges per line, gives us 2000 edges per rev.
 *
 * The period of the timer is configurable by specifying the load value
 * for the timer in the QEILOAD register. We can calculate RPM with
 * the following equation:
 *
 *	RPM = (clock * (2 ^ VelDiv) * Speed * 60) / (Load * PPR * Edges)
 *
 * For our case, consider a motor running at 600 rpm. A 500 pulse per
 * revolution quadrature encoder is attached to the motor, producing 2000
 * phase edges per revolution. With a velocity pre-divider of ÷1
 * (VelDiv set to 0) and clocking on both PhA and PhB edges, this results
 * in 20,000 pulses per second (the motor turns 10 times per second).
 * If the timer were clocked at 80,000,000 Hz, and the load value was
 * 20,000,000 (1/4 of a second), it would count 20000 pulses per update.
 *
 *	RPM = (50,000,000 * 1 * s * 60) / (500,000 * 360 * 4) = 600 rpm
 *	RPM = (100 * s) / 24 = 600 rpm
 *	RPM = (25 * s) / 6 = 600 rpm
 *
 *	RPM = (80,000,000 * 1 * s * 60) / (800,000 * 500 * 4) = 600 rpm
 *	RPM = (100 * s) / 33.33 = 600 rpm
 */

/* The AMS AS5047D quadrature encoder is 2000 step per revolution,
 * or 500 pulses per revolution (PPR).
 */

#define QEI_BASE_SUPPLY	    QEI0_BASE   	/* QEI-0 is SUPPLY encoder */
#define QEI_BASE_TAKEUP	    QEI1_BASE   	/* QEI-1 is TAKEUP encoder */

//#define QE_AS5047D_EDGES    (500 * 4)       /* "D" version is 500 CPR  */
#define QE_AS5047P_EDGES    (1024 * 4)      /* "P" version is 1024 CPR */

//#define QE_EDGES_PER_REV	(QE_PPR * 4)	/* PPR x 4 for four quad encoder edges */
#define QE_TIMER_PERIOD		800000			/* period of 800,000 is 10ms at 80MHz  */

/* Calculate RPM from the velocity value */
#define RPM(s)				((25 * s) / 6)

void ReelQEI_initialize(void);
