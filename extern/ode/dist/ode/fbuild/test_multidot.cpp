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
#include "ode/ode.h"

#define NUM_A 2
#define ALLOCA dALLOCA16
#define SIZE 1000


extern "C" void dMultidot2 (const dReal *a0, const dReal *a1,
			    const dReal *b, dReal *outsum, int n);
/*
extern "C" void dMultidot4 (const dReal *a0, const dReal *a1,
			    const dReal *a2, const dReal *a3,
			    const dReal *b, dReal *outsum, int n);
*/


// correct dot product, for accuracy testing

dReal goodDot (dReal *a, dReal *b, int n)
{
  dReal sum=0;
  while (n > 0) {
    sum += (*a) * (*b);
    a++;
    b++;
    n--;
  }
  return sum;
}


// test multi-dot product accuracy

void testAccuracy()
{
  int j;

  // allocate vectors a and b and fill them with random data
  dReal *a[NUM_A];
  for (j=0; j<NUM_A; j++) a[j] = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  dReal *b = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  for (j=0; j<NUM_A; j++) dMakeRandomMatrix (a[j],1,SIZE,1.0);
  dMakeRandomMatrix (b,1,SIZE,1.0);

  for (int n=1; n<100; n++) {
    dReal good[NUM_A];
    for (j=0; j<NUM_A; j++) good[j] = goodDot (a[j],b,n);
    dReal test[4];
    dMultidot2 (a[0],a[1],b,test,n);
    dReal diff = 0;
    for (j=0; j<NUM_A; j++) diff += fabs(good[j]-test[j]);
    // printf ("diff = %e\n",diff);
    if (diff > 1e-10) printf ("ERROR: accuracy test failed\n");
  }
}


// test multi-dot product factorizer speed.

void testSpeed()
{
  int j;
  dReal sum[NUM_A];

  // allocate vectors a and b and fill them with random data
  dReal *a[NUM_A];
  for (j=0; j<NUM_A; j++) a[j] = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  dReal *b = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  for (j=0; j<NUM_A; j++) dMakeRandomMatrix (a[j],1,SIZE,1.0);
  dMakeRandomMatrix (b,1,SIZE,1.0);

  // time several dot products, return the minimum timing
  double mintime = 1e100;
  dStopwatch sw;
  for (int i=0; i<1000; i++) {
    dStopwatchReset (&sw);
    dStopwatchStart (&sw);

    // try a bunch of prime sizes up to 101
    dMultidot2 (a[0],a[1],b,sum,2);
    dMultidot2 (a[0],a[1],b,sum,3);
    dMultidot2 (a[0],a[1],b,sum,5);
    dMultidot2 (a[0],a[1],b,sum,7);
    dMultidot2 (a[0],a[1],b,sum,11);
    dMultidot2 (a[0],a[1],b,sum,13);
    dMultidot2 (a[0],a[1],b,sum,17);
    dMultidot2 (a[0],a[1],b,sum,19);
    dMultidot2 (a[0],a[1],b,sum,23);
    dMultidot2 (a[0],a[1],b,sum,29);
    dMultidot2 (a[0],a[1],b,sum,31);
    dMultidot2 (a[0],a[1],b,sum,37);
    dMultidot2 (a[0],a[1],b,sum,41);
    dMultidot2 (a[0],a[1],b,sum,43);
    dMultidot2 (a[0],a[1],b,sum,47);
    dMultidot2 (a[0],a[1],b,sum,53);
    dMultidot2 (a[0],a[1],b,sum,59);
    dMultidot2 (a[0],a[1],b,sum,61);
    dMultidot2 (a[0],a[1],b,sum,67);
    dMultidot2 (a[0],a[1],b,sum,71);
    dMultidot2 (a[0],a[1],b,sum,73);
    dMultidot2 (a[0],a[1],b,sum,79);
    dMultidot2 (a[0],a[1],b,sum,83);
    dMultidot2 (a[0],a[1],b,sum,89);
    dMultidot2 (a[0],a[1],b,sum,97);
    dMultidot2 (a[0],a[1],b,sum,101);

    dStopwatchStop (&sw);
    double time = dStopwatchTime (&sw);
    if (time < mintime) mintime = time;
  }

  printf ("%.0f",mintime * dTimerTicksPerSecond());
}


int main()
{
  testAccuracy();
  testSpeed();
  return 0;
}
