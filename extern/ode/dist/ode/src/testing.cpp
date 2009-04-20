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

#include <ode/config.h>
#include <ode/misc.h>
#include <ode/memory.h>
#include "testing.h"

#ifdef dDOUBLE
static const dReal tol = 1.0e-9;
#else
static const dReal tol = 1.0e-5f;
#endif


// matrix header on the stack

struct dMatrixComparison::dMatInfo {
  int n,m;		// size of matrix
  char name[128];	// name of the matrix
  dReal *data;		// matrix data
  int size;		// size of `data'
};



dMatrixComparison::dMatrixComparison()
{
  afterfirst = 0;
  index = 0;
}


dMatrixComparison::~dMatrixComparison()
{
  reset();
}


dReal dMatrixComparison::nextMatrix (dReal *A, int n, int m, int lower_tri,
				     char *name, ...)
{
  if (A==0 || n < 1 || m < 1 || name==0) dDebug (0,"bad args to nextMatrix");
  int num = n*dPAD(m);

  if (afterfirst==0) {
    dMatInfo *mi = (dMatInfo*) dAlloc (sizeof(dMatInfo));
    mi->n = n;
    mi->m = m;
    mi->size = num * sizeof(dReal);
    mi->data = (dReal*) dAlloc (mi->size);
    memcpy (mi->data,A,mi->size);

    va_list ap;
    va_start (ap,name);
    vsprintf (mi->name,name,ap);
    if (strlen(mi->name) >= sizeof (mi->name)) dDebug (0,"name too long");

    mat.push (mi);
    return 0;
  }
  else {
    if (lower_tri && n != m)
      dDebug (0,"dMatrixComparison, lower triangular matrix must be square");
    if (index >= mat.size()) dDebug (0,"dMatrixComparison, too many matrices");
    dMatInfo *mp = mat[index];
    index++;

    dMatInfo mi;
    va_list ap;
    va_start (ap,name);
    vsprintf (mi.name,name,ap);
    if (strlen(mi.name) >= sizeof (mi.name)) dDebug (0,"name too long");

    if (strcmp(mp->name,mi.name) != 0)
      dDebug (0,"dMatrixComparison, name mismatch (\"%s\" and \"%s\")",
	      mp->name,mi.name);
    if (mp->n != n || mp->m != m)
      dDebug (0,"dMatrixComparison, size mismatch (%dx%d and %dx%d)",
	      mp->n,mp->m,n,m);

    dReal maxdiff;
    if (lower_tri) {
      maxdiff = dMaxDifferenceLowerTriangle (A,mp->data,n);
    }
    else {
      maxdiff = dMaxDifference (A,mp->data,n,m);
    }
    if (maxdiff > tol)
      dDebug (0,"dMatrixComparison, matrix error (size=%dx%d, name=\"%s\", "
	      "error=%.4e)",n,m,mi.name,maxdiff);
    return maxdiff;
  }
}


void dMatrixComparison::end()
{
  if (mat.size() <= 0) dDebug (0,"no matrices in sequence");
  afterfirst = 1;
  index = 0;
}


void dMatrixComparison::reset()
{
  for (int i=0; i<mat.size(); i++) {
    dFree (mat[i]->data,mat[i]->size);
    dFree (mat[i],sizeof(dMatInfo));
  }
  mat.setSize (0);
  afterfirst = 0;
  index = 0;
}


void dMatrixComparison::dump()
{
  for (int i=0; i<mat.size(); i++)
    printf ("%d: %s (%dx%d)\n",i,mat[i]->name,mat[i]->n,mat[i]->m);
}

//****************************************************************************
// unit test

#include <setjmp.h>

static jmp_buf jump_buffer;

static void myDebug (int num, const char *msg, va_list ap)
{
  // printf ("(Error %d: ",num);
  // vprintf (msg,ap);
  // printf (")\n");
  longjmp (jump_buffer,1);
}


extern "C" void dTestMatrixComparison()
{
  volatile int i;
  printf ("dTestMatrixComparison()\n");
  dMessageFunction *orig_debug = dGetDebugHandler();

  dMatrixComparison mc;
  dReal A[50*50];

  // make first sequence
  unsigned long seed = dRandGetSeed();
  for (i=1; i<49; i++) {
    dMakeRandomMatrix (A,i,i+1,1.0);
    mc.nextMatrix (A,i,i+1,0,"A%d",i);
  }
  mc.end();

  //mc.dump();

  // test identical sequence
  dSetDebugHandler (&myDebug);
  dRandSetSeed (seed);
  if (setjmp (jump_buffer)) {
    printf ("\tFAILED (1)\n");
  }
  else {
    for (i=1; i<49; i++) {
      dMakeRandomMatrix (A,i,i+1,1.0);
      mc.nextMatrix (A,i,i+1,0,"A%d",i);
    }
    mc.end();
    printf ("\tpassed (1)\n");
  }
  dSetDebugHandler (orig_debug);

  // test broken sequences (with matrix error)
  dRandSetSeed (seed);
  volatile int passcount = 0;
  for (i=1; i<49; i++) {
    if (setjmp (jump_buffer)) {
      passcount++;
    }
    else {
      dSetDebugHandler (&myDebug);
      dMakeRandomMatrix (A,i,i+1,1.0);
      A[(i-1)*dPAD(i+1)+i] += REAL(0.01);
      mc.nextMatrix (A,i,i+1,0,"A%d",i);
      dSetDebugHandler (orig_debug);
    }
  }
  mc.end();
  printf ("\t%s (2)\n",(passcount == 48) ? "passed" : "FAILED");

  // test broken sequences (with name error)
  dRandSetSeed (seed);
  passcount = 0;
  for (i=1; i<49; i++) {
    if (setjmp (jump_buffer)) {
      passcount++;
    }
    else {
      dSetDebugHandler (&myDebug);
      dMakeRandomMatrix (A,i,i+1,1.0);
      mc.nextMatrix (A,i,i+1,0,"B%d",i);
      dSetDebugHandler (orig_debug);
    }
  }
  mc.end();
  printf ("\t%s (3)\n",(passcount == 48) ? "passed" : "FAILED");

  // test identical sequence again
  dSetDebugHandler (&myDebug);
  dRandSetSeed (seed);
  if (setjmp (jump_buffer)) {
    printf ("\tFAILED (4)\n");
  }
  else {
    for (i=1; i<49; i++) {
      dMakeRandomMatrix (A,i,i+1,1.0);
      mc.nextMatrix (A,i,i+1,0,"A%d",i);
    }
    mc.end();
    printf ("\tpassed (4)\n");
  }
  dSetDebugHandler (orig_debug);
}
