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

#ifndef _ODE_SPACE_H_
#define _ODE_SPACE_H_

#include <ode/common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dContactGeom;

typedef void dNearCallback (void *data, dGeomID o1, dGeomID o2);


/* extra information the space needs in every geometry object */

typedef struct dGeomSpaceData {
  dGeomID next;
} dGeomSpaceData;


dSpaceID dSimpleSpaceCreate();
dSpaceID dHashSpaceCreate();

void dSpaceDestroy (dSpaceID);
void dSpaceAdd (dSpaceID, dGeomID);
void dSpaceRemove (dSpaceID, dGeomID);
void dSpaceCollide (dSpaceID space, void *data, dNearCallback *callback);
int dSpaceQuery (dSpaceID, dGeomID);

void dHashSpaceSetLevels (dSpaceID space, int minlevel, int maxlevel);


/* @@@ NOT FLEXIBLE ENOUGH
 *
 * generate contacts for those objects in the space that touch each other.
 * an array of contacts is created on the alternative stack using
 * StackAlloc(), and a pointer to the array is returned. the size of the
 * array is returned by the function.
 */
/* int dSpaceCollide (dSpaceID space, dContactGeom **contact_array); */


/* HMMMMM... i dont think so.
 * tell the space that an object has moved, so its representation in the
 * space should be changed.
 */
/* void dSpaceObjectMoved (dSpaceID, dGeomID); */


#ifdef __cplusplus
}
#endif

#endif
