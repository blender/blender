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

/*

quaternions have the format: (s,vx,vy,vz) where (vx,vy,vz) is the
"rotation axis" and s is the "rotation angle".

*/

#include <ode/rotation.h>


#define _R(i,j) R[(i)*4+(j)]

#define SET_3x3_IDENTITY \
  _R(0,0) = REAL(1.0); \
  _R(0,1) = REAL(0.0); \
  _R(0,2) = REAL(0.0); \
  _R(0,3) = REAL(0.0); \
  _R(1,0) = REAL(0.0); \
  _R(1,1) = REAL(1.0); \
  _R(1,2) = REAL(0.0); \
  _R(1,3) = REAL(0.0); \
  _R(2,0) = REAL(0.0); \
  _R(2,1) = REAL(0.0); \
  _R(2,2) = REAL(1.0); \
  _R(2,3) = REAL(0.0);


void dRSetIdentity (dMatrix3 R)
{
  dAASSERT (R);
  SET_3x3_IDENTITY;
}


void dRFromAxisAndAngle (dMatrix3 R, dReal ax, dReal ay, dReal az,
			 dReal angle)
{
  dAASSERT (R);
  dQuaternion q;
  dQFromAxisAndAngle (q,ax,ay,az,angle);
  dQtoR (q,R);
}


void dRFromEulerAngles (dMatrix3 R, dReal phi, dReal theta, dReal psi)
{
  dReal sphi,cphi,stheta,ctheta,spsi,cpsi;
  dAASSERT (R);
  sphi = dSin(phi);
  cphi = dCos(phi);
  stheta = dSin(theta);
  ctheta = dCos(theta);
  spsi = dSin(psi);
  cpsi = dCos(psi);
  _R(0,0) = cpsi*ctheta;
  _R(0,1) = spsi*ctheta;
  _R(0,2) =-stheta;
  _R(1,0) = cpsi*stheta*sphi - spsi*cphi;
  _R(1,1) = spsi*stheta*sphi + cpsi*cphi;
  _R(1,2) = ctheta*sphi;
  _R(2,0) = cpsi*stheta*cphi + spsi*sphi;
  _R(2,1) = spsi*stheta*cphi - cpsi*sphi;
  _R(2,2) = ctheta*cphi;
}


void dRFrom2Axes (dMatrix3 R, dReal ax, dReal ay, dReal az,
		  dReal bx, dReal by, dReal bz)
{
  dReal l,k;
  dAASSERT (R);
  l = dSqrt (ax*ax + ay*ay + az*az);
  if (l <= REAL(0.0)) {
    dDEBUGMSG ("zero length vector");
    return;
  }
  l = dRecip(l);
  ax *= l;
  ay *= l;
  az *= l;
  k = ax*bx + ay*by + az*bz;
  bx -= k*ax;
  by -= k*ay;
  bz -= k*az;
  l = dSqrt (bx*bx + by*by + bz*bz);
  if (l <= REAL(0.0)) {
    dDEBUGMSG ("zero length vector");
    return;
  }
  l = dRecip(l);
  bx *= l;
  by *= l;
  bz *= l;
  _R(0,0) = ax;
  _R(1,0) = ay;
  _R(2,0) = az;
  _R(0,1) = bx;
  _R(1,1) = by;
  _R(2,1) = bz;
  _R(0,2) = - by*az + ay*bz;
  _R(1,2) = - bz*ax + az*bx;
  _R(2,2) = - bx*ay + ax*by;
}


void dQSetIdentity (dQuaternion q)
{
  dAASSERT (q);
  q[0] = 1;
  q[1] = 0;
  q[2] = 0;
  q[3] = 0;
}


void dQFromAxisAndAngle (dQuaternion q, dReal ax, dReal ay, dReal az,
			 dReal angle)
{
  dAASSERT (q);
  dReal l = ax*ax + ay*ay + az*az;
  if (l > REAL(0.0)) {
    angle *= REAL(0.5);
    q[0] = dCos (angle);
    l = dSin(angle) * dRecipSqrt(l);
    q[1] = ax*l;
    q[2] = ay*l;
    q[3] = az*l;
  }
  else {
    q[0] = 1;
    q[1] = 0;
    q[2] = 0;
    q[3] = 0;
  }
}


void dQMultiply0 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc)
{
  dAASSERT (qa && qb && qc);
  qa[0] = qb[0]*qc[0] - qb[1]*qc[1] - qb[2]*qc[2] - qb[3]*qc[3];
  qa[1] = qb[0]*qc[1] + qb[1]*qc[0] + qb[2]*qc[3] - qb[3]*qc[2];
  qa[2] = qb[0]*qc[2] + qb[2]*qc[0] + qb[3]*qc[1] - qb[1]*qc[3];
  qa[3] = qb[0]*qc[3] + qb[3]*qc[0] + qb[1]*qc[2] - qb[2]*qc[1];
}


void dQMultiply1 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc)
{
  dAASSERT (qa && qb && qc);
  qa[0] = qb[0]*qc[0] + qb[1]*qc[1] + qb[2]*qc[2] + qb[3]*qc[3];
  qa[1] = qb[0]*qc[1] - qb[1]*qc[0] - qb[2]*qc[3] + qb[3]*qc[2];
  qa[2] = qb[0]*qc[2] - qb[2]*qc[0] - qb[3]*qc[1] + qb[1]*qc[3];
  qa[3] = qb[0]*qc[3] - qb[3]*qc[0] - qb[1]*qc[2] + qb[2]*qc[1];
}


void dQMultiply2 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc)
{
  dAASSERT (qa && qb && qc);
  qa[0] =  qb[0]*qc[0] + qb[1]*qc[1] + qb[2]*qc[2] + qb[3]*qc[3];
  qa[1] = -qb[0]*qc[1] + qb[1]*qc[0] - qb[2]*qc[3] + qb[3]*qc[2];
  qa[2] = -qb[0]*qc[2] + qb[2]*qc[0] - qb[3]*qc[1] + qb[1]*qc[3];
  qa[3] = -qb[0]*qc[3] + qb[3]*qc[0] - qb[1]*qc[2] + qb[2]*qc[1];
}


void dQMultiply3 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc)
{
  dAASSERT (qa && qb && qc);
  qa[0] =  qb[0]*qc[0] - qb[1]*qc[1] - qb[2]*qc[2] - qb[3]*qc[3];
  qa[1] = -qb[0]*qc[1] - qb[1]*qc[0] + qb[2]*qc[3] - qb[3]*qc[2];
  qa[2] = -qb[0]*qc[2] - qb[2]*qc[0] + qb[3]*qc[1] - qb[1]*qc[3];
  qa[3] = -qb[0]*qc[3] - qb[3]*qc[0] + qb[1]*qc[2] - qb[2]*qc[1];
}


// QtoR(), RtoQ() and WtoDQ() are derived from equations in "An Introduction
// to Physically Based Modeling: Rigid Body Simulation - 1: Unconstrained
// Rigid Body Dynamics" by David Baraff, Robotics Institute, Carnegie Mellon
// University, 1997.

void dQtoR (const dQuaternion q, dMatrix3 R)
{
  dAASSERT (q && R);
  // q = (s,vx,vy,vz)
  dReal qq1 = 2*q[1]*q[1];
  dReal qq2 = 2*q[2]*q[2];
  dReal qq3 = 2*q[3]*q[3];
  _R(0,0) = 1 - qq2 - qq3;
  _R(0,1) = 2*(q[1]*q[2] - q[0]*q[3]);
  _R(0,2) = 2*(q[1]*q[3] + q[0]*q[2]);
  _R(1,0) = 2*(q[1]*q[2] + q[0]*q[3]);
  _R(1,1) = 1 - qq1 - qq3;
  _R(1,2) = 2*(q[2]*q[3] - q[0]*q[1]);
  _R(2,0) = 2*(q[1]*q[3] - q[0]*q[2]);
  _R(2,1) = 2*(q[2]*q[3] + q[0]*q[1]);
  _R(2,2) = 1 - qq1 - qq2;
}


void dRtoQ (const dMatrix3 R, dQuaternion q)
{
  dAASSERT (q && R);
  dReal tr,s;
  tr = _R(0,0) + _R(1,1) + _R(2,2);
  if (tr >= 0) {
    s = dSqrt (tr + 1);
    q[0] = REAL(0.5) * s;
    s = REAL(0.5) * dRecip(s);
    q[1] = (_R(2,1) - _R(1,2)) * s;
    q[2] = (_R(0,2) - _R(2,0)) * s;
    q[3] = (_R(1,0) - _R(0,1)) * s;
  }
  else {
    // find the largest diagonal element and jump to the appropriate case
    if (_R(1,1) > _R(0,0)) {
      if (_R(2,2) > _R(1,1)) goto case_2;
      goto case_1;
    }
    if (_R(2,2) > _R(0,0)) goto case_2;
    goto case_0;

    case_0:
    s = dSqrt((_R(0,0) - (_R(1,1) + _R(2,2))) + 1);
    q[1] = REAL(0.5) * s;
    s = REAL(0.5) * dRecip(s);
    q[2] = (_R(0,1) + _R(1,0)) * s;
    q[3] = (_R(2,0) + _R(0,2)) * s;
    q[0] = (_R(2,1) - _R(1,2)) * s;
    return;

    case_1:
    s = dSqrt((_R(1,1) - (_R(2,2) + _R(0,0))) + 1);
    q[2] = REAL(0.5) * s;
    s = REAL(0.5) * dRecip(s);
    q[3] = (_R(1,2) + _R(2,1)) * s;
    q[1] = (_R(0,1) + _R(1,0)) * s;
    q[0] = (_R(0,2) - _R(2,0)) * s;
    return;

    case_2:
    s = dSqrt((_R(2,2) - (_R(0,0) + _R(1,1))) + 1);
    q[3] = REAL(0.5) * s;
    s = REAL(0.5) * dRecip(s);
    q[1] = (_R(2,0) + _R(0,2)) * s;
    q[2] = (_R(1,2) + _R(2,1)) * s;
    q[0] = (_R(1,0) - _R(0,1)) * s;
    return;
  }
}


void dWtoDQ (const dVector3 w, const dQuaternion q, dVector4 dq)
{
  dAASSERT (w && q && dq);
  dq[0] = REAL(0.5)*(- w[0]*q[1] - w[1]*q[2] - w[2]*q[3]);
  dq[1] = REAL(0.5)*(  w[0]*q[0] + w[1]*q[3] - w[2]*q[2]);
  dq[2] = REAL(0.5)*(- w[0]*q[3] + w[1]*q[0] + w[2]*q[1]);
  dq[3] = REAL(0.5)*(  w[0]*q[2] - w[1]*q[1] + w[2]*q[0]);
}
