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
#include <ode/matrix.h>
#include <ode/error.h>
#include <ode/memory.h>
#include "mat.h"


dMatrix::dMatrix()
{
  n = 0;
  m = 0;
  data = 0;
}


dMatrix::dMatrix (int rows, int cols)
{
  if (rows < 1 || cols < 1) dDebug (0,"bad matrix size");
  n = rows;
  m = cols;
  data = (dReal*) dAlloc (n*m*sizeof(dReal));
  dSetZero (data,n*m);
}


dMatrix::dMatrix (const dMatrix &a)
{
  n = a.n;
  m = a.m;
  data = (dReal*) dAlloc (n*m*sizeof(dReal));
  memcpy (data,a.data,n*m*sizeof(dReal));
}


dMatrix::dMatrix (int rows, int cols,
		    dReal *_data, int rowskip, int colskip)
{
  if (rows < 1 || cols < 1) dDebug (0,"bad matrix size");
  n = rows;
  m = cols;
  data = (dReal*) dAlloc (n*m*sizeof(dReal));
  for (int i=0; i<n; i++) {
    for (int j=0; j<m; j++) data[i*m+j] = _data[i*rowskip + j*colskip];
  }
}


dMatrix::~dMatrix()
{
  if (data) dFree (data,n*m*sizeof(dReal));
}


dReal & dMatrix::operator () (int i, int j)
{
  if (i < 0 || i >= n || j < 0 || j >= m) dDebug (0,"bad matrix (i,j)");
  return data [i*m+j];
}


void dMatrix::operator= (const dMatrix &a)
{
  if (data) dFree (data,n*m*sizeof(dReal));
  n = a.n;
  m = a.m;
  if (n > 0 && m > 0) {
    data = (dReal*) dAlloc (n*m*sizeof(dReal));
    memcpy (data,a.data,n*m*sizeof(dReal));
  }
  else data = 0;
}


void dMatrix::operator= (dReal a)
{
  for (int i=0; i<n*m; i++) data[i] = a;
}


dMatrix dMatrix::transpose()
{
  dMatrix r (m,n);
  for (int i=0; i<n; i++) {
    for (int j=0; j<m; j++) r.data[j*n+i] = data[i*m+j];
  }
  return r;
}


dMatrix dMatrix::select (int np, int *p, int nq, int *q)
{
  if (np < 1 || nq < 1) dDebug (0,"Matrix select, bad index array sizes");
  dMatrix r (np,nq);
  for (int i=0; i<np; i++) {
    for (int j=0; j<nq; j++) {
      if (p[i] < 0 || p[i] >= n || q[i] < 0 || q[i] >= m)
	dDebug (0,"Matrix select, bad index arrays");
      r.data[i*nq+j] = data[p[i]*m+q[j]];
    }
  }
  return r;
}


dMatrix dMatrix::operator + (const dMatrix &a)
{
  if (n != a.n || m != a.m) dDebug (0,"matrix +, mismatched sizes");
  dMatrix r (n,m);
  for (int i=0; i<n*m; i++) r.data[i] = data[i] + a.data[i];
  return r;
}


dMatrix dMatrix::operator - (const dMatrix &a)
{
  if (n != a.n || m != a.m) dDebug (0,"matrix -, mismatched sizes");
  dMatrix r (n,m);
  for (int i=0; i<n*m; i++) r.data[i] = data[i] - a.data[i];
  return r;
}


dMatrix dMatrix::operator - ()
{
  dMatrix r (n,m);
  for (int i=0; i<n*m; i++) r.data[i] = -data[i];
  return r;
}


dMatrix dMatrix::operator * (const dMatrix &a)
{
  if (m != a.n) dDebug (0,"matrix *, mismatched sizes");
  dMatrix r (n,a.m);
  for (int i=0; i<n; i++) {
    for (int j=0; j<a.m; j++) {
      dReal sum = 0;
      for (int k=0; k<m; k++) sum += data[i*m+k] * a.data[k*a.m+j];
      r.data [i*a.m+j] = sum;
    }
  }
  return r;
}


void dMatrix::operator += (const dMatrix &a)
{
  if (n != a.n || m != a.m) dDebug (0,"matrix +=, mismatched sizes");
  for (int i=0; i<n*m; i++) data[i] += a.data[i];
}


void dMatrix::operator -= (const dMatrix &a)
{
  if (n != a.n || m != a.m) dDebug (0,"matrix -=, mismatched sizes");
  for (int i=0; i<n*m; i++) data[i] -= a.data[i];
}


void dMatrix::clearUpperTriangle()
{
  if (n != m) dDebug (0,"clearUpperTriangle() only works on square matrices");
  for (int i=0; i<n; i++) {
    for (int j=i+1; j<m; j++) data[i*m+j] = 0;
  }
}


void dMatrix::clearLowerTriangle()
{
  if (n != m) dDebug (0,"clearLowerTriangle() only works on square matrices");
  for (int i=0; i<n; i++) {
    for (int j=0; j<i; j++) data[i*m+j] = 0;
  }
}


void dMatrix::makeRandom (dReal range)
{
  for (int i=0; i<n; i++) {
    for (int j=0; j<m; j++)
      data[i*m+j] = (dRandReal()*REAL(2.0)-REAL(1.0))*range;
  }
}


void dMatrix::print (char *fmt, FILE *f)
{
  for (int i=0; i<n; i++) {
    for (int j=0; j<m; j++) fprintf (f,fmt,data[i*m+j]);
    fprintf (f,"\n");
  }
}


dReal dMatrix::maxDifference (const dMatrix &a)
{
  if (n != a.n || m != a.m) dDebug (0,"maxDifference(), mismatched sizes");
  dReal max = 0;
  for (int i=0; i<n; i++) {
    for (int j=0; j<m; j++) {
      dReal diff = dFabs(data[i*m+j] - a.data[i*m+j]);
      if (diff > max) max = diff;
    }
  }
  return max;
}
