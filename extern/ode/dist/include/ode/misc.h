/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

/* miscellaneous math functions. these are mostly useful for testing */

#ifndef _ODE_MISC_H_
#define _ODE_MISC_H_

#include <ode/common.h>


#ifdef __cplusplus
extern "C" {
#endif


/* return 1 if the random number generator is working. */
int dTestRand();

/* return next 32 bit random number. this uses a not-very-random linear
 * congruential method.
 */
unsigned long dRand();

/* get and set the current random number seed. */
unsigned long  dRandGetSeed();
void dRandSetSeed (unsigned long s);

/* return a random integer between 0..n-1. the distribution will get worse
 * as n approaches 2^32.
 */
int dRandInt (int n);

/* return a random real number between 0..1 */
dReal dRandReal();

/* print out a matrix */
#ifdef __cplusplus
void dPrintMatrix (dReal *A, int n, int m, char *fmt = "%10.4f ",
		   FILE *f=stdout);
#else
void dPrintMatrix (dReal *A, int n, int m, char *fmt, FILE *f);
#endif

/* make a random vector with entries between +/- range. A has n elements. */
void dMakeRandomVector (dReal *A, int n, dReal range);

/* make a random matrix with entries between +/- range. A has size n*m. */
void dMakeRandomMatrix (dReal *A, int n, int m, dReal range);

/* clear the upper triangle of a square matrix */
void dClearUpperTriangle (dReal *A, int n);

/* return the maximum element difference between the two n*m matrices */
dReal dMaxDifference (const dReal *A, const dReal *B, int n, int m);

/* return the maximum element difference between the lower triangle of two
 * n*n matrices */
dReal dMaxDifferenceLowerTriangle (const dReal *A, const dReal *B, int n);


#ifdef __cplusplus
}
#endif

#endif
