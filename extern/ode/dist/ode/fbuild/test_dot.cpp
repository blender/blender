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

#define ALLOCA dALLOCA16
#define SIZE 1000


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


// test dot product accuracy

void testAccuracy()
{
  // allocate vectors a and b and fill them with random data
  dReal *a = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  dReal *b = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  dMakeRandomMatrix (a,1,SIZE,1.0);
  dMakeRandomMatrix (b,1,SIZE,1.0);

  for (int n=1; n<100; n++) {
    dReal good = goodDot (a,b,n);
    dReal test = dDot (a,b,n);
    dReal diff = fabs(good-test);
    //printf ("diff = %e\n",diff);
    if (diff > 1e-10) printf ("ERROR: accuracy test failed\n");
  }
}


// test dot product factorizer speed.

void testSpeed()
{
  // allocate vectors a and b and fill them with random data
  dReal *a = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  dReal *b = (dReal*) ALLOCA (SIZE*sizeof(dReal));
  dMakeRandomMatrix (a,1,SIZE,1.0);
  dMakeRandomMatrix (b,1,SIZE,1.0);

  // time several dot products, return the minimum timing
  double mintime = 1e100;
  dStopwatch sw;
  for (int i=0; i<1000; i++) {
    dStopwatchReset (&sw);
    dStopwatchStart (&sw);

    // try a bunch of prime sizes up to 101
    dDot (a,b,2);
    dDot (a,b,3);
    dDot (a,b,5);
    dDot (a,b,7);
    dDot (a,b,11);
    dDot (a,b,13);
    dDot (a,b,17);
    dDot (a,b,19);
    dDot (a,b,23);
    dDot (a,b,29);
    dDot (a,b,31);
    dDot (a,b,37);
    dDot (a,b,41);
    dDot (a,b,43);
    dDot (a,b,47);
    dDot (a,b,53);
    dDot (a,b,59);
    dDot (a,b,61);
    dDot (a,b,67);
    dDot (a,b,71);
    dDot (a,b,73);
    dDot (a,b,79);
    dDot (a,b,83);
    dDot (a,b,89);
    dDot (a,b,97);
    dDot (a,b,101);

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
