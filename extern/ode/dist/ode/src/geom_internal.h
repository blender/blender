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

#ifndef _ODE_GEOM_INTERNAL_H_
#define _ODE_GEOM_INTERNAL_H_


// mask for the number-of-contacts field in the dCollide() flags parameter
#define NUMC_MASK (0xffff)


// internal info for geometry class

struct dxGeomClass {
  dGetColliderFnFn *collider;
  dGetAABBFn *aabb;
  dAABBTestFn *aabb_test;
  dGeomDtorFn *dtor;
  int num;		// class number
  int size;		// total size of object, including extra data area
};


// position vector and rotation matrix for geometry objects that are not
// connected to bodies.

struct dxPosR {
  dVector3 pos;
  dMatrix3 R;
};


// common data for all geometry objects. the class-specific data area follows
// this structure. pos and R will either point to a separately allocated
// buffer (if body is 0 - pos points to the dxPosR object) or to the pos and
// R of the body (if body nonzero).

struct dxGeom {		// a dGeomID is a pointer to this
  dxGeomClass *_class;	// class of this object
  void *data;		// user data pointer
  dBodyID body;		// dynamics body associated with this object (if any)
  dReal *pos;		// pointer to object's position vector
  dReal *R;		// pointer to object's rotation matrix
  dSpaceID spaceid;	// the space this object is in
  dGeomSpaceData space;	// reserved for use by space this object is in
  dReal *space_aabb;	// ptr to aabb array held by dSpaceCollide() fn
  // class-specific data follows here, with proper alignment.
};


// this is the size of the dxGeom structure rounded up to a multiple of 16
// bytes. any class specific data that comes after this will have the correct
// alignment.

#define SIZEOF_DXGEOM dEFFICIENT_SIZE(sizeof(dxGeom))


// given a pointer to a dxGeom, return a pointer to the class data that
// follows it.

#define CLASSDATA(geomptr) (((char*)geomptr) + SIZEOF_DXGEOM)



#endif
