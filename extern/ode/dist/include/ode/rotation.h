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

#ifndef _ODE_ROTATION_H_
#define _ODE_ROTATION_H_

#include <ode/common.h>

#ifdef __cplusplus
extern "C" {
#endif


void dRSetIdentity (dMatrix3 R);

void dRFromAxisAndAngle (dMatrix3 R, dReal ax, dReal ay, dReal az,
			 dReal angle);

void dRFromEulerAngles (dMatrix3 R, dReal phi, dReal theta, dReal psi);

void dRFrom2Axes (dMatrix3 R, dReal ax, dReal ay, dReal az,
		  dReal bx, dReal by, dReal bz);

void dQSetIdentity (dQuaternion q);

void dQFromAxisAndAngle (dQuaternion q, dReal ax, dReal ay, dReal az,
			 dReal angle);

void dQMultiply0 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc);
void dQMultiply1 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc);
void dQMultiply2 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc);
void dQMultiply3 (dQuaternion qa, const dQuaternion qb, const dQuaternion qc);

void dQtoR (const dQuaternion q, dMatrix3 R);

void dRtoQ (const dMatrix3 R, dQuaternion q);

void dWtoDQ (const dVector3 w, const dQuaternion q, dVector4 dq);


#ifdef __cplusplus
}
#endif

#endif
