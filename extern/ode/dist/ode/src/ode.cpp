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

// this source file is mostly concerned with the data structures, not the
// numerics.

#include "objects.h"
#include <ode/ode.h>
#include "joint.h"
#include <ode/odemath.h>
#include <ode/matrix.h>
#include "step.h"
#include <ode/memory.h>
#include <ode/error.h>

// misc defines
#define ALLOCA dALLOCA16

//****************************************************************************
// utility

static inline void initObject (dObject *obj, dxWorld *w)
{
  obj->world = w;
  obj->next = 0;
  obj->tome = 0;
  obj->userdata = 0;
  obj->tag = 0;
}


// add an object `obj' to the list who's head pointer is pointed to by `first'.

static inline void addObjectToList (dObject *obj, dObject **first)
{
  obj->next = *first;
  obj->tome = first;
  if (*first) (*first)->tome = &obj->next;
  (*first) = obj;
}


// remove the object from the linked list

static inline void removeObjectFromList (dObject *obj)
{
  if (obj->next) obj->next->tome = obj->tome;
  *(obj->tome) = obj->next;
  // safeguard
  obj->next = 0;
  obj->tome = 0;
}


// remove the joint from neighbour lists of all connected bodies

static void removeJointReferencesFromAttachedBodies (dxJoint *j)
{
  for (int i=0; i<2; i++) {
    dxBody *body = j->node[i].body;
    if (body) {
      dxJointNode *n = body->firstjoint;
      dxJointNode *last = 0;
      while (n) {
	if (n->joint == j) {
	  if (last) last->next = n->next;
	  else body->firstjoint = n->next;
	  break;
	}
	last = n;
	n = n->next;
      }
    }
  }
  j->node[0].body = 0;
  j->node[0].next = 0;
  j->node[1].body = 0;
  j->node[1].next = 0;
}

//****************************************************************************
// island processing

// this groups all joints and bodies in a world into islands. all objects
// in an island are reachable by going through connected bodies and joints.
// each island can be simulated separately.
// note that joints that are not attached to anything will not be included
// in any island, an so they do not affect the simulation.
//
// this function starts new island from unvisited bodies. however, it will
// never start a new islands from a disabled body. thus islands of disabled
// bodies will not be included in the simulation. disabled bodies are
// re-enabled if they are found to be part of an active island.

static void processIslands (dxWorld *world, dReal stepsize)
{
  dxBody *b,*bb,**body;
  dxJoint *j,**joint;

  // nothing to do if no bodies
  if (world->nb <= 0) return;

  // make arrays for body and joint lists (for a single island) to go into
  body = (dxBody**) ALLOCA (world->nb * sizeof(dxBody*));
  joint = (dxJoint**) ALLOCA (world->nj * sizeof(dxJoint*));
  int bcount = 0;	// number of bodies in `body'
  int jcount = 0;	// number of joints in `joint'

  // set all body/joint tags to 0
  for (b=world->firstbody; b; b=(dxBody*)b->next) b->tag = 0;
  for (j=world->firstjoint; j; j=(dxJoint*)j->next) j->tag = 0;

  // allocate a stack of unvisited bodies in the island. the maximum size of
  // the stack can be the lesser of the number of bodies or joints, because
  // new bodies are only ever added to the stack by going through untagged
  // joints. all the bodies in the stack must be tagged!
  int stackalloc = (world->nj < world->nb) ? world->nj : world->nb;
  dxBody **stack = (dxBody**) ALLOCA (stackalloc * sizeof(dxBody*));

  for (bb=world->firstbody; bb; bb=(dxBody*)bb->next) {
    // get bb = the next enabled, untagged body, and tag it
    if (bb->tag || (bb->flags & dxBodyDisabled)) continue;
    bb->tag = 1;

    // tag all bodies and joints starting from bb.
    int stacksize = 0;
    b = bb;
    body[0] = bb;
    bcount = 1;
    jcount = 0;
    goto quickstart;
    while (stacksize > 0) {
      b = stack[--stacksize];	// pop body off stack
      body[bcount++] = b;	// put body on body list
      quickstart:

      // traverse and tag all body's joints, add untagged connected bodies
      // to stack
      for (dxJointNode *n=b->firstjoint; n; n=n->next) {
	if (!n->joint->tag) {
	  n->joint->tag = 1;
	  joint[jcount++] = n->joint;
	  if (n->body && !n->body->tag) {
	    n->body->tag = 1;
	    stack[stacksize++] = n->body;
	  }
	}
      }
      dIASSERT(stacksize <= world->nb);
      dIASSERT(stacksize <= world->nj);
    }

    // now do something with body and joint lists
    dInternalStepIsland (world,body,bcount,joint,jcount,stepsize);

    // what we've just done may have altered the body/joint tag values.
    // we must make sure that these tags are nonzero.
    // also make sure all bodies are in the enabled state.
    int i;
    for (i=0; i<bcount; i++) {
      body[i]->tag = 1;
      body[i]->flags &= ~dxBodyDisabled;
    }
    for (i=0; i<jcount; i++) joint[i]->tag = 1;
  }

  // if debugging, check that all objects (except for disabled bodies,
  // unconnected joints, and joints that are connected to disabled bodies)
  // were tagged.
# ifndef dNODEBUG
  for (b=world->firstbody; b; b=(dxBody*)b->next) {
    if (b->flags & dxBodyDisabled) {
      if (b->tag) dDebug (0,"disabled body tagged");
    }
    else {
      if (!b->tag) dDebug (0,"enabled body not tagged");
    }
  }
  for (j=world->firstjoint; j; j=(dxJoint*)j->next) {
    if ((j->node[0].body && (j->node[0].body->flags & dxBodyDisabled)==0) ||
	(j->node[1].body && (j->node[1].body->flags & dxBodyDisabled)==0)) {
      if (!j->tag) dDebug (0,"attached enabled joint not tagged");
    }
    else {
      if (j->tag) dDebug (0,"unattached or disabled joint tagged");
    }
  }
# endif
}

//****************************************************************************
// debugging

// see if an object list loops on itself (if so, it's bad).

static int listHasLoops (dObject *first)
{
  if (first==0 || first->next==0) return 0;
  dObject *a=first,*b=first->next;
  int skip=0;
  while (b) {
    if (a==b) return 1;
    b = b->next;
    if (skip) a = a->next;
    skip ^= 1;
  }
  return 0;
}


// check the validity of the world data structures

static void checkWorld (dxWorld *w)
{
  dxBody *b;
  dxJoint *j;

  // check there are no loops
  if (listHasLoops (w->firstbody)) dDebug (0,"body list has loops");
  if (listHasLoops (w->firstjoint)) dDebug (0,"joint list has loops");

  // check lists are well formed (check `tome' pointers)
  for (b=w->firstbody; b; b=(dxBody*)b->next) {
    if (b->next && b->next->tome != &b->next)
      dDebug (0,"bad tome pointer in body list");
  }
  for (j=w->firstjoint; j; j=(dxJoint*)j->next) {
    if (j->next && j->next->tome != &j->next)
      dDebug (0,"bad tome pointer in joint list");
  }

  // check counts
  int n = 0;
  for (b=w->firstbody; b; b=(dxBody*)b->next) n++;
  if (w->nb != n) dDebug (0,"body count incorrect");
  n = 0;
  for (j=w->firstjoint; j; j=(dxJoint*)j->next) n++;
  if (w->nj != n) dDebug (0,"joint count incorrect");

  // set all tag values to a known value
  static int count = 0;
  count++;
  for (b=w->firstbody; b; b=(dxBody*)b->next) b->tag = count;
  for (j=w->firstjoint; j; j=(dxJoint*)j->next) j->tag = count;

  // check all body/joint world pointers are ok
  for (b=w->firstbody; b; b=(dxBody*)b->next) if (b->world != w)
    dDebug (0,"bad world pointer in body list");
  for (j=w->firstjoint; j; j=(dxJoint*)j->next) if (j->world != w)
    dDebug (0,"bad world pointer in joint list");

  /*
  // check for half-connected joints - actually now these are valid
  for (j=w->firstjoint; j; j=(dxJoint*)j->next) {
    if (j->node[0].body || j->node[1].body) {
      if (!(j->node[0].body && j->node[1].body))
	dDebug (0,"half connected joint found");
    }
  }
  */

  // check that every joint node appears in the joint lists of both bodies it
  // attaches
  for (j=w->firstjoint; j; j=(dxJoint*)j->next) {
    for (int i=0; i<2; i++) {
      if (j->node[i].body) {
	int ok = 0;
	for (dxJointNode *n=j->node[i].body->firstjoint; n; n=n->next) {
	  if (n->joint == j) ok = 1;
	}
	if (ok==0) dDebug (0,"joint not in joint list of attached body");
      }
    }
  }

  // check all body joint lists (correct body ptrs)
  for (b=w->firstbody; b; b=(dxBody*)b->next) {
    for (dxJointNode *n=b->firstjoint; n; n=n->next) {
      if (&n->joint->node[0] == n) {
	if (n->joint->node[1].body != b)
	  dDebug (0,"bad body pointer in joint node of body list (1)");
      }
      else {
	if (n->joint->node[0].body != b)
	  dDebug (0,"bad body pointer in joint node of body list (2)");
      }
      if (n->joint->tag != count) dDebug (0,"bad joint node pointer in body");
    }
  }

  // check all body pointers in joints, check they are distinct
  for (j=w->firstjoint; j; j=(dxJoint*)j->next) {
    if (j->node[0].body && (j->node[0].body == j->node[1].body))
      dDebug (0,"non-distinct body pointers in joint");
    if ((j->node[0].body && j->node[0].body->tag != count) ||
	(j->node[1].body && j->node[1].body->tag != count))
      dDebug (0,"bad body pointer in joint");
  }
}


void dWorldCheck (dxWorld *w)
{
  checkWorld (w);
}

//****************************************************************************
// body

dxBody *dBodyCreate (dxWorld *w)
{
  dAASSERT (w);
  dxBody *b = new dxBody;
  initObject (b,w);
  b->firstjoint = 0;
  b->flags = 0;
  dMassSetParameters (&b->mass,1,0,0,0,1,1,1,0,0,0);
  dSetZero (b->invI,4*3);
  b->invI[0] = 1;
  b->invI[5] = 1;
  b->invI[10] = 1;
  b->invMass = 1;
  dSetZero (b->pos,4);
  dSetZero (b->q,4);
  b->q[0] = 1;
  dRSetIdentity (b->R);
  dSetZero (b->lvel,4);
  dSetZero (b->avel,4);
  dSetZero (b->facc,4);
  dSetZero (b->tacc,4);
  dSetZero (b->finite_rot_axis,4);
  addObjectToList (b,(dObject **) &w->firstbody);
  w->nb++;
  return b;
}


void dBodyDestroy (dxBody *b)
{
  dAASSERT (b);

  // detach all neighbouring joints, then delete this body.
  dxJointNode *n = b->firstjoint;
  while (n) {
    // sneaky trick to speed up removal of joint references (black magic)
    n->joint->node[(n == n->joint->node)].body = 0;

    dxJointNode *next = n->next;
    n->next = 0;
    removeJointReferencesFromAttachedBodies (n->joint);
    n = next;
  }
  removeObjectFromList (b);
  b->world->nb--;
  delete b;
}


void dBodySetData (dBodyID b, void *data)
{
  dAASSERT (b);
  b->userdata = data;
}


void *dBodyGetData (dBodyID b)
{
  dAASSERT (b);
  return b->userdata;
}


void dBodySetPosition (dBodyID b, dReal x, dReal y, dReal z)
{
  dAASSERT (b);
  b->pos[0] = x;
  b->pos[1] = y;
  b->pos[2] = z;
}


void dBodySetRotation (dBodyID b, const dMatrix3 R)
{
  dAASSERT (b && R);
  dQuaternion q;
  dRtoQ (R,q);
  dNormalize4 (q);
  b->q[0] = q[0];
  b->q[1] = q[1];
  b->q[2] = q[2];
  b->q[3] = q[3];
  dQtoR (b->q,b->R);
}


void dBodySetQuaternion (dBodyID b, const dQuaternion q)
{
  dAASSERT (b && q);
  b->q[0] = q[0];
  b->q[1] = q[1];
  b->q[2] = q[2];
  b->q[3] = q[3];
  dNormalize4 (b->q);
  dQtoR (b->q,b->R);
}


void dBodySetLinearVel  (dBodyID b, dReal x, dReal y, dReal z)
{
  dAASSERT (b);
  b->lvel[0] = x;
  b->lvel[1] = y;
  b->lvel[2] = z;
}


void dBodySetAngularVel (dBodyID b, dReal x, dReal y, dReal z)
{
  dAASSERT (b);
  b->avel[0] = x;
  b->avel[1] = y;
  b->avel[2] = z;
}


const dReal * dBodyGetPosition (dBodyID b)
{
  dAASSERT (b);
  return b->pos;
}


const dReal * dBodyGetRotation (dBodyID b)
{
  dAASSERT (b);
  return b->R;
}


const dReal * dBodyGetQuaternion (dBodyID b)
{
  dAASSERT (b);
  return b->q;
}


const dReal * dBodyGetLinearVel (dBodyID b)
{
  dAASSERT (b);
  return b->lvel;
}


const dReal * dBodyGetAngularVel (dBodyID b)
{
  dAASSERT (b);
  return b->avel;
}


void dBodySetMass (dBodyID b, const dMass *mass)
{
  dAASSERT (b && mass);
  memcpy (&b->mass,mass,sizeof(dMass));
  if (dInvertPDMatrix (b->mass.I,b->invI,3)==0) {
    dDEBUGMSG ("inertia must be positive definite");
    dRSetIdentity (b->invI);
  }
  b->invMass = dRecip(b->mass.mass);
}


void dBodyGetMass (dBodyID b, dMass *mass)
{
  dAASSERT (b && mass);
  memcpy (mass,&b->mass,sizeof(dMass));
}


void dBodyAddForce (dBodyID b, dReal fx, dReal fy, dReal fz)
{
  dAASSERT (b);
  b->facc[0] += fx;
  b->facc[1] += fy;
  b->facc[2] += fz;
}


void dBodyAddTorque (dBodyID b, dReal fx, dReal fy, dReal fz)
{
  dAASSERT (b);
  b->tacc[0] += fx;
  b->tacc[1] += fy;
  b->tacc[2] += fz;
}


void dBodyAddRelForce (dBodyID b, dReal fx, dReal fy, dReal fz)
{
  dAASSERT (b);
  dVector3 t1,t2;
  t1[0] = fx;
  t1[1] = fy;
  t1[2] = fz;
  t1[3] = 0;
  dMULTIPLY0_331 (t2,b->R,t1);
  b->facc[0] += t2[0];
  b->facc[1] += t2[1];
  b->facc[2] += t2[2];
}


void dBodyAddRelTorque (dBodyID b, dReal fx, dReal fy, dReal fz)
{
  dAASSERT (b);
  dVector3 t1,t2;
  t1[0] = fx;
  t1[1] = fy;
  t1[2] = fz;
  t1[3] = 0;
  dMULTIPLY0_331 (t2,b->R,t1);
  b->tacc[0] += t2[0];
  b->tacc[1] += t2[1];
  b->tacc[2] += t2[2];
}


void dBodyAddForceAtPos (dBodyID b, dReal fx, dReal fy, dReal fz,
			 dReal px, dReal py, dReal pz)
{
  dAASSERT (b);
  b->facc[0] += fx;
  b->facc[1] += fy;
  b->facc[2] += fz;
  dVector3 f,q;
  f[0] = fx;
  f[1] = fy;
  f[2] = fz;
  q[0] = px - b->pos[0];
  q[1] = py - b->pos[1];
  q[2] = pz - b->pos[2];
  dCROSS (b->tacc,+=,q,f);
}


void dBodyAddForceAtRelPos (dBodyID b, dReal fx, dReal fy, dReal fz,
			    dReal px, dReal py, dReal pz)
{
  dAASSERT (b);
  dVector3 prel,f,p;
  f[0] = fx;
  f[1] = fy;
  f[2] = fz;
  f[3] = 0;
  prel[0] = px;
  prel[1] = py;
  prel[2] = pz;
  prel[3] = 0;
  dMULTIPLY0_331 (p,b->R,prel);
  b->facc[0] += f[0];
  b->facc[1] += f[1];
  b->facc[2] += f[2];
  dCROSS (b->tacc,+=,p,f);
}


void dBodyAddRelForceAtPos (dBodyID b, dReal fx, dReal fy, dReal fz,
			    dReal px, dReal py, dReal pz)
{
  dAASSERT (b);
  dVector3 frel,f;
  frel[0] = fx;
  frel[1] = fy;
  frel[2] = fz;
  frel[3] = 0;
  dMULTIPLY0_331 (f,b->R,frel);
  b->facc[0] += f[0];
  b->facc[1] += f[1];
  b->facc[2] += f[2];
  dVector3 q;
  q[0] = px - b->pos[0];
  q[1] = py - b->pos[1];
  q[2] = pz - b->pos[2];
  dCROSS (b->tacc,+=,q,f);
}


void dBodyAddRelForceAtRelPos (dBodyID b, dReal fx, dReal fy, dReal fz,
			       dReal px, dReal py, dReal pz)
{
  dAASSERT (b);
  dVector3 frel,prel,f,p;
  frel[0] = fx;
  frel[1] = fy;
  frel[2] = fz;
  frel[3] = 0;
  prel[0] = px;
  prel[1] = py;
  prel[2] = pz;
  prel[3] = 0;
  dMULTIPLY0_331 (f,b->R,frel);
  dMULTIPLY0_331 (p,b->R,prel);
  b->facc[0] += f[0];
  b->facc[1] += f[1];
  b->facc[2] += f[2];
  dCROSS (b->tacc,+=,p,f);
}


const dReal * dBodyGetForce (dBodyID b)
{
  dAASSERT (b);
  return b->facc;
}


const dReal * dBodyGetTorque (dBodyID b)
{
  dAASSERT (b);
  return b->tacc;
}


void dBodySetForce (dBodyID b, dReal x, dReal y, dReal z)
{
  dAASSERT (b);
  b->facc[0] = x;
  b->facc[1] = y;
  b->facc[2] = z;
}


void dBodySetTorque (dBodyID b, dReal x, dReal y, dReal z)
{
  dAASSERT (b);
  b->tacc[0] = x;
  b->tacc[1] = y;
  b->tacc[2] = z;
}


void dBodyGetRelPointPos (dBodyID b, dReal px, dReal py, dReal pz,
			  dVector3 result)
{
  dAASSERT (b);
  dVector3 prel,p;
  prel[0] = px;
  prel[1] = py;
  prel[2] = pz;
  prel[3] = 0;
  dMULTIPLY0_331 (p,b->R,prel);
  result[0] = p[0] + b->pos[0];
  result[1] = p[1] + b->pos[1];
  result[2] = p[2] + b->pos[2];
}


void dBodyGetRelPointVel (dBodyID b, dReal px, dReal py, dReal pz,
			  dVector3 result)
{
  dAASSERT (b);
  dVector3 prel,p;
  prel[0] = px;
  prel[1] = py;
  prel[2] = pz;
  prel[3] = 0;
  dMULTIPLY0_331 (p,b->R,prel);
  result[0] = b->lvel[0];
  result[1] = b->lvel[1];
  result[2] = b->lvel[2];
  dCROSS (result,+=,b->avel,p);
}


void dBodyGetPointVel (dBodyID b, dReal px, dReal py, dReal pz,
		       dVector3 result)
{
  dAASSERT (b);
  dVector3 p;
  p[0] = px - b->pos[0];
  p[1] = py - b->pos[1];
  p[2] = pz - b->pos[2];
  p[3] = 0;
  result[0] = b->lvel[0];
  result[1] = b->lvel[1];
  result[2] = b->lvel[2];
  dCROSS (result,+=,b->avel,p);
}


void dBodyGetPosRelPoint (dBodyID b, dReal px, dReal py, dReal pz,
			  dVector3 result)
{
  dAASSERT (b);
  dVector3 prel;
  prel[0] = px - b->pos[0];
  prel[1] = py - b->pos[1];
  prel[2] = pz - b->pos[2];
  prel[3] = 0;
  dMULTIPLY1_331 (result,b->R,prel);
}


void dBodyVectorToWorld (dBodyID b, dReal px, dReal py, dReal pz,
			 dVector3 result)
{
  dAASSERT (b);
  dVector3 p;
  p[0] = px;
  p[1] = py;
  p[2] = pz;
  p[3] = 0;
  dMULTIPLY0_331 (result,b->R,p);
}


void dBodyVectorFromWorld (dBodyID b, dReal px, dReal py, dReal pz,
			   dVector3 result)
{
  dAASSERT (b);
  dVector3 p;
  p[0] = px;
  p[1] = py;
  p[2] = pz;
  p[3] = 0;
  dMULTIPLY1_331 (result,b->R,p);
}


void dBodySetFiniteRotationMode (dBodyID b, int mode)
{
  dAASSERT (b);
  b->flags &= ~(dxBodyFlagFiniteRotation | dxBodyFlagFiniteRotationAxis);
  if (mode) {
    b->flags |= dxBodyFlagFiniteRotation;
    if (b->finite_rot_axis[0] != 0 || b->finite_rot_axis[1] != 0 ||
	b->finite_rot_axis[2] != 0) {
      b->flags |= dxBodyFlagFiniteRotationAxis;
    }
  }
}


void dBodySetFiniteRotationAxis (dBodyID b, dReal x, dReal y, dReal z)
{
  dAASSERT (b);
  b->finite_rot_axis[0] = x;
  b->finite_rot_axis[1] = y;
  b->finite_rot_axis[2] = z;
  if (x != 0 || y != 0 || z != 0) {
    dNormalize3 (b->finite_rot_axis);
    b->flags |= dxBodyFlagFiniteRotationAxis;
  }
  else {
    b->flags &= ~dxBodyFlagFiniteRotationAxis;
  }
}


int dBodyGetFiniteRotationMode (dBodyID b)
{
  dAASSERT (b);
  return ((b->flags & dxBodyFlagFiniteRotation) != 0);
}


void dBodyGetFiniteRotationAxis (dBodyID b, dVector3 result)
{
  dAASSERT (b);
  result[0] = b->finite_rot_axis[0];
  result[1] = b->finite_rot_axis[1];
  result[2] = b->finite_rot_axis[2];
}


int dBodyGetNumJoints (dBodyID b)
{
  dAASSERT (b);
  int count=0;
  for (dxJointNode *n=b->firstjoint; n; n=n->next, count++);
  return count;
}


dJointID dBodyGetJoint (dBodyID b, int index)
{
  dAASSERT (b);
  int i=0;
  for (dxJointNode *n=b->firstjoint; n; n=n->next, i++) {
    if (i == index) return n->joint;
  }
  return 0;
}


void dBodyEnable (dBodyID b)
{
  dAASSERT (b);
  b->flags &= ~dxBodyDisabled;
}


void dBodyDisable (dBodyID b)
{
  dAASSERT (b);
  b->flags |= dxBodyDisabled;
}


int dBodyIsEnabled (dBodyID b)
{
  dAASSERT (b);
  return ((b->flags & dxBodyDisabled) == 0);
}


void dBodySetGravityMode (dBodyID b, int mode)
{
  dAASSERT (b);
  if (mode) b->flags &= ~dxBodyNoGravity;
  else b->flags |= dxBodyNoGravity;
}


int dBodyGetGravityMode (dBodyID b)
{
  dAASSERT (b);
  return ((b->flags & dxBodyNoGravity) == 0);
}

//****************************************************************************
// joints

static void dJointInit (dxWorld *w, dxJoint *j)
{
  dIASSERT (w && j);
  initObject (j,w);
  j->vtable = 0;
  j->flags = 0;
  j->node[0].joint = j;
  j->node[0].body = 0;
  j->node[0].next = 0;
  j->node[1].joint = j;
  j->node[1].body = 0;
  j->node[1].next = 0;
  addObjectToList (j,(dObject **) &w->firstjoint);
  w->nj++;
}


static dxJoint *createJoint (dWorldID w, dJointGroupID group,
			     dxJoint::Vtable *vtable)
{
  dIASSERT (w && vtable);
  dxJoint *j;
  if (group) {
    j = (dxJoint*) group->stack.alloc (vtable->size);
    group->num++;
  }
  else j = (dxJoint*) dAlloc (vtable->size);
  dJointInit (w,j);
  j->vtable = vtable;
  if (group) j->flags |= dJOINT_INGROUP;
  if (vtable->init) vtable->init (j);
  j->feedback = 0;
  return j;
}


dxJoint * dJointCreateBall (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__dball_vtable);
}


dxJoint * dJointCreateHinge (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__dhinge_vtable);
}


dxJoint * dJointCreateSlider (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__dslider_vtable);
}


dxJoint * dJointCreateContact (dWorldID w, dJointGroupID group,
			       const dContact *c)
{
  dAASSERT (w && c);
  dxJointContact *j = (dxJointContact *)
    createJoint (w,group,&__dcontact_vtable);
  j->contact = *c;
  return j;
}


dxJoint * dJointCreateHinge2 (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__dhinge2_vtable);
}


dxJoint * dJointCreateUniversal (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__duniversal_vtable);
}


dxJoint * dJointCreateFixed (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__dfixed_vtable);
}


dxJoint * dJointCreateNull (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__dnull_vtable);
}


dxJoint * dJointCreateAMotor (dWorldID w, dJointGroupID group)
{
  dAASSERT (w);
  return createJoint (w,group,&__damotor_vtable);
}


void dJointDestroy (dxJoint *j)
{
  dAASSERT (j);
  if (j->flags & dJOINT_INGROUP) return;
  removeJointReferencesFromAttachedBodies (j);
  removeObjectFromList (j);
  j->world->nj--;
  dFree (j,j->vtable->size);
}


dJointGroupID dJointGroupCreate (int max_size)
{
  // not any more ... dUASSERT (max_size > 0,"max size must be > 0");
  dxJointGroup *group = new dxJointGroup;
  group->num = 0;
  return group;
}


void dJointGroupDestroy (dJointGroupID group)
{
  dAASSERT (group);
  dJointGroupEmpty (group);
  delete group;
}


void dJointGroupEmpty (dJointGroupID group)
{
  // the joints in this group are detached starting from the most recently
  // added (at the top of the stack). this helps ensure that the various
  // linked lists are not traversed too much, as the joints will hopefully
  // be at the start of those lists.
  // if any group joints have their world pointer set to 0, their world was
  // previously destroyed. no special handling is required for these joints.

  dAASSERT (group);
  int i;
  dxJoint **jlist = (dxJoint**) ALLOCA (group->num * sizeof(dxJoint*));
  dxJoint *j = (dxJoint*) group->stack.rewind();
  for (i=0; i < group->num; i++) {
    jlist[i] = j;
    j = (dxJoint*) (group->stack.next (j->vtable->size));
  }
  for (i=group->num-1; i >= 0; i--) {
    if (jlist[i]->world) {
      removeJointReferencesFromAttachedBodies (jlist[i]);
      removeObjectFromList (jlist[i]);
      jlist[i]->world->nj--;
    }
  }
  group->num = 0;
  group->stack.freeAll();
}


void dJointAttach (dxJoint *joint, dxBody *body1, dxBody *body2)
{
  // check arguments
  dUASSERT (joint,"bad joint argument");
  dUASSERT (body1 == 0 || body1 != body2,"can't have body1==body2");
  dxWorld *world = joint->world;
  dUASSERT ( (!body1 || body1->world == world) &&
	     (!body2 || body2->world == world),
	     "joint and bodies must be in same world");

  // check if the joint can not be attached to just one body
  dUASSERT (!((joint->flags & dJOINT_TWOBODIES) &&
	      ((body1 != 0) ^ (body2 != 0))),
	    "joint can not be attached to just one body");

  // remove any existing body attachments
  if (joint->node[0].body || joint->node[1].body) {
    removeJointReferencesFromAttachedBodies (joint);
  }

  // if a body is zero, make sure that it is body2, so 0 --> node[1].body
  if (body1==0) {
    body1 = body2;
    body2 = 0;
    joint->flags &= (~dJOINT_REVERSE);
  }
  else {
    joint->flags |= dJOINT_REVERSE;
  }

  // attach to new bodies
  joint->node[0].body = body1;
  joint->node[1].body = body2;
  if (body1) {
    joint->node[1].next = body1->firstjoint;
    body1->firstjoint = &joint->node[1];
  }
  else joint->node[1].next = 0;
  if (body2) {
    joint->node[0].next = body2->firstjoint;
    body2->firstjoint = &joint->node[0];
  }
  else {
    joint->node[0].next = 0;
  }
}


void dJointSetData (dxJoint *joint, void *data)
{
  dAASSERT (joint);
  joint->userdata = data;
}


void *dJointGetData (dxJoint *joint)
{
  dAASSERT (joint);
  return joint->userdata;
}


int dJointGetType (dxJoint *joint)
{
  dAASSERT (joint);
  return joint->vtable->typenum;
}


dBodyID dJointGetBody (dxJoint *joint, int index)
{
  dAASSERT (joint);
  if (index >= 0 && index < 2) return joint->node[index].body;
  else return 0;
}


void dJointSetFeedback (dxJoint *joint, dJointFeedback *f)
{
  dAASSERT (joint);
  joint->feedback = f;
}


dJointFeedback *dJointGetFeedback (dxJoint *joint)
{
  dAASSERT (joint);
  return joint->feedback;
}


int dAreConnected (dBodyID b1, dBodyID b2)
{
  dAASSERT (b1 && b2);
  // look through b1's neighbour list for b2
  for (dxJointNode *n=b1->firstjoint; n; n=n->next) {
    if (n->body == b2) return 1;
  }
  return 0;
}

//****************************************************************************
// world

dxWorld * dWorldCreate()
{
  dxWorld *w = new dxWorld;
  w->firstbody = 0;
  w->firstjoint = 0;
  w->nb = 0;
  w->nj = 0;
  dSetZero (w->gravity,4);
  w->global_erp = REAL(0.2);
#if defined(dSINGLE)
  w->global_cfm = 1e-5f;
#elif defined(dDOUBLE)
  w->global_cfm = 1e-10;
#else
  #error dSINGLE or dDOUBLE must be defined
#endif
  return w;
}


void dWorldDestroy (dxWorld *w)
{
  // delete all bodies and joints
  dAASSERT (w);
  dxBody *nextb, *b = w->firstbody;
  while (b) {
    nextb = (dxBody*) b->next;
    delete b;
    b = nextb;
  }
  dxJoint *nextj, *j = w->firstjoint;
  while (j) {
    nextj = (dxJoint*)j->next;
    if (j->flags & dJOINT_INGROUP) {
      // the joint is part of a group, so "deactivate" it instead
      j->world = 0;
      j->node[0].body = 0;
      j->node[0].next = 0;
      j->node[1].body = 0;
      j->node[1].next = 0;
      dMessage (0,"warning: destroying world containing grouped joints");
    }
    else {
      dFree (j,j->vtable->size);
    }
    j = nextj;
  }
  delete w;
}


void dWorldSetGravity (dWorldID w, dReal x, dReal y, dReal z)
{
  dAASSERT (w);
  w->gravity[0] = x;
  w->gravity[1] = y;
  w->gravity[2] = z;
}


void dWorldGetGravity (dWorldID w, dVector3 g)
{
  dAASSERT (w);
  g[0] = w->gravity[0];
  g[1] = w->gravity[1];
  g[2] = w->gravity[2];
}


void dWorldSetERP (dWorldID w, dReal erp)
{
  dAASSERT (w);
  w->global_erp = erp;
}


dReal dWorldGetERP (dWorldID w)
{
  dAASSERT (w);
  return w->global_erp;
}


void dWorldSetCFM (dWorldID w, dReal cfm)
{
  dAASSERT (w);
  w->global_cfm = cfm;
}


dReal dWorldGetCFM (dWorldID w)
{
  dAASSERT (w);
  return w->global_cfm;
}


void dWorldStep (dWorldID w, dReal stepsize)
{
  dUASSERT (w,"bad world argument");
  dUASSERT (stepsize > 0,"stepsize must be > 0");
  processIslands (w,stepsize);
}


void dWorldImpulseToForce (dWorldID w, dReal stepsize,
			   dReal ix, dReal iy, dReal iz,
			   dVector3 force)
{
  dAASSERT (w);
  stepsize = dRecip(stepsize);
  force[0] = stepsize * ix;
  force[1] = stepsize * iy;
  force[2] = stepsize * iz;
  // @@@ force[3] = 0;
}

//****************************************************************************
// testing

#define NUM 100

#define DO(x)


extern "C" void dTestDataStructures()
{
  int i;
  DO(printf ("testDynamicsStuff()\n"));

  dBodyID body [NUM];
  int nb = 0;
  dJointID joint [NUM];
  int nj = 0;

  for (i=0; i<NUM; i++) body[i] = 0;
  for (i=0; i<NUM; i++) joint[i] = 0;

  DO(printf ("creating world\n"));
  dWorldID w = dWorldCreate();
  checkWorld (w);

  for (;;) {
    if (nb < NUM && dRandReal() > 0.5) {
      DO(printf ("creating body\n"));
      body[nb] = dBodyCreate (w);
      DO(printf ("\t--> %p\n",body[nb]));
      nb++;
      checkWorld (w);
      DO(printf ("%d BODIES, %d JOINTS\n",nb,nj));
    }
    if (nj < NUM && nb > 2 && dRandReal() > 0.5) {
      dBodyID b1 = body [dRand() % nb];
      dBodyID b2 = body [dRand() % nb];
      if (b1 != b2) {
	DO(printf ("creating joint, attaching to %p,%p\n",b1,b2));
	joint[nj] = dJointCreateBall (w,0);
	DO(printf ("\t-->%p\n",joint[nj]));
	checkWorld (w);
	dJointAttach (joint[nj],b1,b2);
	nj++;
	checkWorld (w);
	DO(printf ("%d BODIES, %d JOINTS\n",nb,nj));
      }
    }
    if (nj > 0 && nb > 2 && dRandReal() > 0.5) {
      dBodyID b1 = body [dRand() % nb];
      dBodyID b2 = body [dRand() % nb];
      if (b1 != b2) {
	int k = dRand() % nj;
	DO(printf ("reattaching joint %p\n",joint[k]));
	dJointAttach (joint[k],b1,b2);
	checkWorld (w);
	DO(printf ("%d BODIES, %d JOINTS\n",nb,nj));
      }
    }
    if (nb > 0 && dRandReal() > 0.5) {
      int k = dRand() % nb;
      DO(printf ("destroying body %p\n",body[k]));
      dBodyDestroy (body[k]);
      checkWorld (w);
      for (; k < (NUM-1); k++) body[k] = body[k+1];
      nb--;
      DO(printf ("%d BODIES, %d JOINTS\n",nb,nj));
    }
    if (nj > 0 && dRandReal() > 0.5) {
      int k = dRand() % nj;
      DO(printf ("destroying joint %p\n",joint[k]));
      dJointDestroy (joint[k]);
      checkWorld (w);
      for (; k < (NUM-1); k++) joint[k] = joint[k+1];
      nj--;
      DO(printf ("%d BODIES, %d JOINTS\n",nb,nj));
    }
  }

  /*
  printf ("creating world\n");
  dWorldID w = dWorldCreate();
  checkWorld (w);
  printf ("creating body\n");
  dBodyID b1 = dBodyCreate (w);
  checkWorld (w);
  printf ("creating body\n");
  dBodyID b2 = dBodyCreate (w);
  checkWorld (w);
  printf ("creating joint\n");
  dJointID j = dJointCreateBall (w);
  checkWorld (w);
  printf ("attaching joint\n");
  dJointAttach (j,b1,b2);
  checkWorld (w);
  printf ("destroying joint\n");
  dJointDestroy (j);
  checkWorld (w);
  printf ("destroying body\n");
  dBodyDestroy (b1);
  checkWorld (w);
  printf ("destroying body\n");
  dBodyDestroy (b2);
  checkWorld (w);
  printf ("destroying world\n");
  dWorldDestroy (w);
  */
}
