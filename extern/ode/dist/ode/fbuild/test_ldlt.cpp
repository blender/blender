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

#include <stdio.h>
#include <malloc.h>
#include "ode/ode.h"

#define ALLOCA dALLOCA16

//****************************************************************************
// constants

#ifdef dSINGLE
#define TOL (1e-4)
#else
#define TOL (1e-10)
#endif

//****************************************************************************
// test L*X=B solver accuracy.

void testSolverAccuracy (int n)
{
  int i;
  int npad = dPAD(n);
  dReal *L = (dReal*) ALLOCA (n*npad*sizeof(dReal));
  dReal *B = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *B2 = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *X = (dReal*) ALLOCA (n*sizeof(dReal));

  // L is a random lower triangular matrix with 1's on the diagonal
  dMakeRandomMatrix (L,n,n,1.0);
  dClearUpperTriangle (L,n);
  for (i=0; i<n; i++) L[i*npad+i] = 1;

  // B is the right hand side
  dMakeRandomMatrix (B,n,1,1.0);
  memcpy (X,B,n*sizeof(dReal));	// copy B to X

  dSolveL1 (L,X,n,npad);

  /*
  dPrintMatrix (L,n,n);
  printf ("\n");
  dPrintMatrix (B,n,1);
  printf ("\n");
  dPrintMatrix (X,n,1);
  printf ("\n");
  */

  dSetZero (B2,n);
  dMultiply0 (B2,L,X,n,n,1);
  dReal error = dMaxDifference (B,B2,1,n);
  if (error > TOL) {
    printf ("error = %e, size = %d\n",error,n);
  }
}

//****************************************************************************
// test L^T*X=B solver accuracy.

void testTransposeSolverAccuracy (int n)
{
  int i;
  int npad = dPAD(n);
  dReal *L = (dReal*) ALLOCA (n*npad*sizeof(dReal));
  dReal *B = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *B2 = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *X = (dReal*) ALLOCA (n*sizeof(dReal));

  // L is a random lower triangular matrix with 1's on the diagonal
  dMakeRandomMatrix (L,n,n,1.0);
  dClearUpperTriangle (L,n);
  for (i=0; i<n; i++) L[i*npad+i] = 1;

  // B is the right hand side
  dMakeRandomMatrix (B,n,1,1.0);
  memcpy (X,B,n*sizeof(dReal));	// copy B to X

  dSolveL1T (L,X,n,npad);

  dSetZero (B2,n);
  dMultiply1 (B2,L,X,n,n,1);
  dReal error = dMaxDifference (B,B2,1,n);
  if (error > TOL) {
    printf ("error = %e, size = %d\n",error,n);
  }
}

//****************************************************************************
// test L*D*L' factorizer accuracy.

void testLDLTAccuracy (int n)
{
  int i,j;
  int npad = dPAD(n);
  dReal *A = (dReal*) ALLOCA (n*npad*sizeof(dReal));
  dReal *L = (dReal*) ALLOCA (n*npad*sizeof(dReal));
  dReal *d = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *Atest = (dReal*) ALLOCA (n*npad*sizeof(dReal));
  dReal *DL = (dReal*) ALLOCA (n*npad*sizeof(dReal));

  dMakeRandomMatrix (A,n,n,1.0);
  dMultiply2 (L,A,A,n,n,n);
  memcpy (A,L,n*npad*sizeof(dReal));
  dSetZero (d,n);

  dFactorLDLT (L,d,n,npad);

  // make L lower triangular, and convert d into diagonal of D
  dClearUpperTriangle (L,n);
  for (i=0; i<n; i++) L[i*npad+i] = 1;
  for (i=0; i<n; i++) d[i] = 1.0/d[i];

  // form Atest = L*D*L'
  dSetZero (Atest,n*npad);
  dSetZero (DL,n*npad);
  for (i=0; i<n; i++) {
    for (j=0; j<n; j++) DL[i*npad+j] = L[i*npad+j] * d[j];
  }
  dMultiply2 (Atest,L,DL,n,n,n);
  dReal error = dMaxDifference (A,Atest,n,n);
  if (error > TOL) {
    printf ("error = %e, size = %d\n",error,n);
  }

  /*
  printf ("\n");
  dPrintMatrix (A,n,n);
  printf ("\n");
  dPrintMatrix (L,n,n);
  printf ("\n");
  dPrintMatrix (d,1,n);
  */
}

//****************************************************************************
// test L*D*L' factorizer speed.

void testLDLTSpeed (int n)
{
  int npad = dPAD(n);

  // allocate A
  dReal *A = (dReal*) ALLOCA (n*npad*sizeof(dReal));

  // make B a symmetric positive definite matrix
  dMakeRandomMatrix (A,n,n,1.0);
  dReal *B = (dReal*) ALLOCA (n*npad*sizeof(dReal));
  dSetZero (B,n*npad);
  dMultiply2 (B,A,A,n,n,n);

  // make d
  dReal *d = (dReal*) ALLOCA (n*sizeof(dReal));
  dSetZero (d,n);

  // time several factorizations, return the minimum timing
  double mintime = 1e100;
  dStopwatch sw;
  for (int i=0; i<100; i++) {
    memcpy (A,B,n*npad*sizeof(dReal));
    dStopwatchReset (&sw);
    dStopwatchStart (&sw);

    dFactorLDLT (A,d,n,npad);

    dStopwatchStop (&sw);
    double time = dStopwatchTime (&sw);
    if (time < mintime) mintime = time;
  }

  printf ("%.0f",mintime * dTimerTicksPerSecond());
}

//****************************************************************************
// test solver speed.

void testSolverSpeed (int n, int transpose)
{
  int i;
  int npad = dPAD(n);

  // allocate L,B,X
  dReal *L = (dReal*) ALLOCA (n*npad*sizeof(dReal));
  dReal *B = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *X = (dReal*) ALLOCA (n*sizeof(dReal));

  // L is a random lower triangular matrix with 1's on the diagonal
  dMakeRandomMatrix (L,n,n,1.0);
  dClearUpperTriangle (L,n);
  for (i=0; i<n; i++) L[i*npad+i] = 1;

  // B is the right hand side
  dMakeRandomMatrix (B,n,1,1.0);

  // time several factorizations, return the minimum timing
  double mintime = 1e100;
  dStopwatch sw;
  for (int i=0; i<100; i++) {
    memcpy (X,B,n*sizeof(dReal));	// copy B to X

    if (transpose) {
      dStopwatchReset (&sw);
      dStopwatchStart (&sw);
      dSolveL1T (L,X,n,npad);
      dStopwatchStop (&sw);
    }
    else {
      dStopwatchReset (&sw);
      dStopwatchStart (&sw);
      dSolveL1 (L,X,n,npad);
      dStopwatchStop (&sw);
    }

    double time = dStopwatchTime (&sw);
    if (time < mintime) mintime = time;
  }

  printf ("%.0f",mintime * dTimerTicksPerSecond());
}

//****************************************************************************
// the single command line argument is 'f' to test and time the factorizer,
// or 's' to test and time the solver.


void testAccuracy (int n, char type)
{
  if (type == 'f') testLDLTAccuracy (n);
  if (type == 's') testSolverAccuracy (n);
  if (type == 't') testTransposeSolverAccuracy (n);
}


void testSpeed (int n, char type)
{
  if (type == 'f') testLDLTSpeed (n);
  if (type == 's') testSolverSpeed (n,0);
  if (type == 't') testSolverSpeed (n,1);
}


int main (int argc, char **argv)
{
  if (argc != 2 || argv[1][0] == 0 || argv[1][1] != 0 ||
      (argv[1][0] != 'f' && argv[1][0] != 's' && argv[1][0] != 't')) {
    fprintf (stderr,"Usage: test_ldlt [f|s|t]\n");
    exit (1);
  }
  char type = argv[1][0];

  // accuracy test: test all sizes up to 20 then all prime sizes up to 101
  int i;
  for (i=1; i<20; i++) {
    testAccuracy (i,type);
  }
  testAccuracy (23,type);
  testAccuracy (29,type);
  testAccuracy (31,type);
  testAccuracy (37,type);
  testAccuracy (41,type);
  testAccuracy (43,type);
  testAccuracy (47,type);
  testAccuracy (53,type);
  testAccuracy (59,type);
  testAccuracy (61,type);
  testAccuracy (67,type);
  testAccuracy (71,type);
  testAccuracy (73,type);
  testAccuracy (79,type);
  testAccuracy (83,type);
  testAccuracy (89,type);
  testAccuracy (97,type);
  testAccuracy (101,type);

  // test speed on a 127x127 matrix
  testSpeed (127,type);

  return 0;
}
