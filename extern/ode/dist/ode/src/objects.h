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

// object, body, and world structs.


#ifndef _ODE_OBJECT_H_
#define _ODE_OBJECT_H_

#include <ode/common.h>
#include <ode/memory.h>
#include <ode/mass.h>
#include "array.h"


// some body flags

enum {
  dxBodyFlagFiniteRotation = 1,		// use finite rotations
  dxBodyFlagFiniteRotationAxis = 2,	// use finite rotations only along axis
  dxBodyDisabled = 4,			// body is disabled
  dxBodyNoGravity = 8			// body is not influenced by gravity
};


// base class that does correct object allocation / deallocation

struct dBase {
  void *operator new (size_t size) { return dAlloc (size); }
  void operator delete (void *ptr, size_t size) { dFree (ptr,size); }
  void *operator new[] (size_t size) { return dAlloc (size); }
  void operator delete[] (void *ptr, size_t size) { dFree (ptr,size); }
};


// base class for bodies and joints

struct dObject : public dBase {
  dxWorld *world;		// world this object is in
  dObject *next;		// next object of this type in list
  dObject **tome;		// pointer to previous object's next ptr
  void *userdata;		// user settable data
  int tag;			// used by dynamics algorithms
};


struct dxBody : public dObject {
  dxJointNode *firstjoint;	// list of attached joints
  int flags;			// some dxBodyFlagXXX flags
  dMass mass;			// mass parameters about POR
  dMatrix3 invI;		// inverse of mass.I
  dReal invMass;		// 1 / mass.mass
  dVector3 pos;			// position of POR (point of reference)
  dQuaternion q;		// orientation quaternion
  dMatrix3 R;			// rotation matrix, always corresponds to q
  dVector3 lvel,avel;		// linear and angular velocity of POR
  dVector3 facc,tacc;		// force and torque accululators
  dVector3 finite_rot_axis;	// finite rotation axis, unit length or 0=none
};


struct dxWorld : public dBase {
  dxBody *firstbody;		// body linked list
  dxJoint *firstjoint;		// joint linked list
  int nb,nj;			// number of bodies and joints in lists
  dVector3 gravity;		// gravity vector (m/s/s)
  dReal global_erp;		// global error reduction parameter
  dReal global_cfm;		// global costraint force mixing parameter
};


#endif
