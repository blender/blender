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

#include "objects.h"
#include "joint.h"
#include <ode/config.h>
#include <ode/odemath.h>
#include <ode/rotation.h>
#include <ode/timer.h>
#include <ode/error.h>
#include <ode/matrix.h>
#include "lcp.h"

//****************************************************************************
// misc defines

#define FAST_FACTOR
//#define TIMING

#define ALLOCA dALLOCA16

//****************************************************************************
// debugging - comparison of various vectors and matrices produced by the
// slow and fast versions of the stepper.

//#define COMPARE_METHODS

#ifdef COMPARE_METHODS
#include "testing.h"
dMatrixComparison comparator;
#endif

//****************************************************************************
// special matrix multipliers

// this assumes the 4th and 8th rows of B and C are zero.

static void Multiply2_p8r (dReal *A, dReal *B, dReal *C,
			   int p, int r, int Askip)
{
  int i,j;
  dReal sum,*bb,*cc;
  dIASSERT (p>0 && r>0 && A && B && C);
  bb = B;
  for (i=p; i; i--) {
    cc = C;
    for (j=r; j; j--) {
      sum = bb[0]*cc[0];
      sum += bb[1]*cc[1];
      sum += bb[2]*cc[2];
      sum += bb[4]*cc[4];
      sum += bb[5]*cc[5];
      sum += bb[6]*cc[6];
      *(A++) = sum; 
      cc += 8;
    }
    A += Askip - r;
    bb += 8;
  }
}


// this assumes the 4th and 8th rows of B and C are zero.

static void MultiplyAdd2_p8r (dReal *A, dReal *B, dReal *C,
			      int p, int r, int Askip)
{
  int i,j;
  dReal sum,*bb,*cc;
  dIASSERT (p>0 && r>0 && A && B && C);
  bb = B;
  for (i=p; i; i--) {
    cc = C;
    for (j=r; j; j--) {
      sum = bb[0]*cc[0];
      sum += bb[1]*cc[1];
      sum += bb[2]*cc[2];
      sum += bb[4]*cc[4];
      sum += bb[5]*cc[5];
      sum += bb[6]*cc[6];
      *(A++) += sum; 
      cc += 8;
    }
    A += Askip - r;
    bb += 8;
  }
}


// this assumes the 4th and 8th rows of B are zero.

static void Multiply0_p81 (dReal *A, dReal *B, dReal *C, int p)
{
  int i;
  dIASSERT (p>0 && A && B && C);
  dReal sum;
  for (i=p; i; i--) {
    sum =  B[0]*C[0];
    sum += B[1]*C[1];
    sum += B[2]*C[2];
    sum += B[4]*C[4];
    sum += B[5]*C[5];
    sum += B[6]*C[6];
    *(A++) = sum;
    B += 8;
  }
}


// this assumes the 4th and 8th rows of B are zero.

static void MultiplyAdd0_p81 (dReal *A, dReal *B, dReal *C, int p)
{
  int i;
  dIASSERT (p>0 && A && B && C);
  dReal sum;
  for (i=p; i; i--) {
    sum =  B[0]*C[0];
    sum += B[1]*C[1];
    sum += B[2]*C[2];
    sum += B[4]*C[4];
    sum += B[5]*C[5];
    sum += B[6]*C[6];
    *(A++) += sum;
    B += 8;
  }
}


// this assumes the 4th and 8th rows of B are zero.

static void MultiplyAdd1_8q1 (dReal *A, dReal *B, dReal *C, int q)
{
  int k;
  dReal sum;
  dIASSERT (q>0 && A && B && C);
  sum = 0;
  for (k=0; k<q; k++) sum += B[k*8] * C[k];
  A[0] += sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[1+k*8] * C[k];
  A[1] += sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[2+k*8] * C[k];
  A[2] += sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[4+k*8] * C[k];
  A[4] += sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[5+k*8] * C[k];
  A[5] += sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[6+k*8] * C[k];
  A[6] += sum;
}


// this assumes the 4th and 8th rows of B are zero.

static void Multiply1_8q1 (dReal *A, dReal *B, dReal *C, int q)
{
  int k;
  dReal sum;
  dIASSERT (q>0 && A && B && C);
  sum = 0;
  for (k=0; k<q; k++) sum += B[k*8] * C[k];
  A[0] = sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[1+k*8] * C[k];
  A[1] = sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[2+k*8] * C[k];
  A[2] = sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[4+k*8] * C[k];
  A[4] = sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[5+k*8] * C[k];
  A[5] = sum;
  sum = 0;
  for (k=0; k<q; k++) sum += B[6+k*8] * C[k];
  A[6] = sum;
}

//****************************************************************************
// body rotation

// return sin(x)/x. this has a singularity at 0 so special handling is needed
// for small arguments.

static inline dReal sinc (dReal x)
{
  // if |x| < 1e-4 then use a taylor series expansion. this two term expansion
  // is actually accurate to one LS bit within this range if double precision
  // is being used - so don't worry!
  if (dFabs(x) < 1.0e-4) return REAL(1.0) - x*x*REAL(0.166666666666666666667);
  else return dSin(x)/x;
}


// given a body b, apply its linear and angular rotation over the time
// interval h, thereby adjusting its position and orientation.

static inline void moveAndRotateBody (dxBody *b, dReal h)
{
  int j;

  // handle linear velocity
  for (j=0; j<3; j++) b->pos[j] += h * b->lvel[j];

  if (b->flags & dxBodyFlagFiniteRotation) {
    dVector3 irv;	// infitesimal rotation vector
    dQuaternion q;	// quaternion for finite rotation

    if (b->flags & dxBodyFlagFiniteRotationAxis) {
      // split the angular velocity vector into a component along the finite
      // rotation axis, and a component orthogonal to it.
      dVector3 frv,irv;		// finite rotation vector
      dReal k = dDOT (b->finite_rot_axis,b->avel);
      frv[0] = b->finite_rot_axis[0] * k;
      frv[1] = b->finite_rot_axis[1] * k;
      frv[2] = b->finite_rot_axis[2] * k;
      irv[0] = b->avel[0] - frv[0];
      irv[1] = b->avel[1] - frv[1];
      irv[2] = b->avel[2] - frv[2];

      // make a rotation quaternion q that corresponds to frv * h.
      // compare this with the full-finite-rotation case below.
      h *= REAL(0.5);
      dReal theta = k * h;
      q[0] = dCos(theta);
      dReal s = sinc(theta) * h;
      q[1] = frv[0] * s;
      q[2] = frv[1] * s;
      q[3] = frv[2] * s;
    }
    else {
      // make a rotation quaternion q that corresponds to w * h
      dReal wlen = dSqrt (b->avel[0]*b->avel[0] + b->avel[1]*b->avel[1] +
			  b->avel[2]*b->avel[2]);
      h *= REAL(0.5);
      dReal theta = wlen * h;
      q[0] = dCos(theta);
      dReal s = sinc(theta) * h;
      q[1] = b->avel[0] * s;
      q[2] = b->avel[1] * s;
      q[3] = b->avel[2] * s;
    }

    // do the finite rotation
    dQuaternion q2;
    dQMultiply0 (q2,q,b->q);
    for (j=0; j<4; j++) b->q[j] = q2[j];

    // do the infitesimal rotation if required
    if (b->flags & dxBodyFlagFiniteRotationAxis) {
      dReal dq[4];
      dWtoDQ (irv,b->q,dq);
      for (j=0; j<4; j++) b->q[j] += h * dq[j];
    }
  }
  else {
    // the normal way - do an infitesimal rotation
    dReal dq[4];
    dWtoDQ (b->avel,b->q,dq);
    for (j=0; j<4; j++) b->q[j] += h * dq[j];
  }

  // normalize the quaternion and convert it to a rotation matrix
  dNormalize4 (b->q);
  dQtoR (b->q,b->R);
}

//****************************************************************************
// the slow, but sure way
// note that this does not do any joint feedback!

// given lists of bodies and joints that form an island, perform a first
// order timestep.
//
// `body' is the body array, `nb' is the size of the array.
// `_joint' is the body array, `nj' is the size of the array.

void dInternalStepIsland_x1 (dxWorld *world, dxBody * const *body, int nb,
			     dxJoint * const *_joint, int nj, dReal stepsize)
{
  int i,j,k;
  int n6 = 6*nb;

# ifdef TIMING
  dTimerStart("preprocessing");
# endif

  // number all bodies in the body list - set their tag values
  for (i=0; i<nb; i++) body[i]->tag = i;

  // make a local copy of the joint array, because we might want to modify it.
  // (the "dxJoint *const*" declaration says we're allowed to modify the joints
  // but not the joint array, because the caller might need it unchanged).
  dxJoint **joint = (dxJoint**) ALLOCA (nj * sizeof(dxJoint*));
  memcpy (joint,_joint,nj * sizeof(dxJoint*));

  // for all bodies, compute the inertia tensor and its inverse in the global
  // frame, and compute the rotational force and add it to the torque
  // accumulator.
  // @@@ check computation of rotational force.
  dReal *I = (dReal*) ALLOCA (3*nb*4 * sizeof(dReal));
  dReal *invI = (dReal*) ALLOCA (3*nb*4 * sizeof(dReal));
  dSetZero (I,3*nb*4);
  dSetZero (invI,3*nb*4);
  for (i=0; i<nb; i++) {
    dReal tmp[12];
    // compute inertia tensor in global frame
    dMULTIPLY2_333 (tmp,body[i]->mass.I,body[i]->R);
    dMULTIPLY0_333 (I+i*12,body[i]->R,tmp);
    // compute inverse inertia tensor in global frame
    dMULTIPLY2_333 (tmp,body[i]->invI,body[i]->R);
    dMULTIPLY0_333 (invI+i*12,body[i]->R,tmp);
    // compute rotational force
    dMULTIPLY0_331 (tmp,I+i*12,body[i]->avel);
    dCROSS (body[i]->tacc,-=,body[i]->avel,tmp);
  }

  // add the gravity force to all bodies
  for (i=0; i<nb; i++) {
    if ((body[i]->flags & dxBodyNoGravity)==0) {
      body[i]->facc[0] += body[i]->mass.mass * world->gravity[0];
      body[i]->facc[1] += body[i]->mass.mass * world->gravity[1];
      body[i]->facc[2] += body[i]->mass.mass * world->gravity[2];
    }
  }

  // get m = total constraint dimension, nub = number of unbounded variables.
  // create constraint offset array and number-of-rows array for all joints.
  // the constraints are re-ordered as follows: the purely unbounded
  // constraints, the mixed unbounded + LCP constraints, and last the purely
  // LCP constraints.
  //
  // joints with m=0 are inactive and are removed from the joints array
  // entirely, so that the code that follows does not consider them.
  int m = 0;
  dxJoint::Info1 *info = (dxJoint::Info1*) ALLOCA (nj*sizeof(dxJoint::Info1));
  int *ofs = (int*) ALLOCA (nj*sizeof(int));
  for (i=0, j=0; j<nj; j++) {	// i=dest, j=src
    joint[j]->vtable->getInfo1 (joint[j],info+i);
    dIASSERT (info[i].m >= 0 && info[i].m <= 6 &&
	      info[i].nub >= 0 && info[i].nub <= info[i].m);
    if (info[i].m > 0) {
      joint[i] = joint[j];
      i++;
    }
  }
  nj = i;

  // the purely unbounded constraints
  for (i=0; i<nj; i++) if (info[i].nub == info[i].m) {
    ofs[i] = m;
    m += info[i].m;
  }
  int nub = m;
  // the mixed unbounded + LCP constraints
  for (i=0; i<nj; i++) if (info[i].nub > 0 && info[i].nub < info[i].m) {
    ofs[i] = m;
    m += info[i].m;
  }
  // the purely LCP constraints
  for (i=0; i<nj; i++) if (info[i].nub == 0) {
    ofs[i] = m;
    m += info[i].m;
  }

  // create (6*nb,6*nb) inverse mass matrix `invM', and fill it with mass
  // parameters
# ifdef TIMING
  dTimerNow ("create mass matrix");
# endif
  int nskip = dPAD (n6);
  dReal *invM = (dReal*) ALLOCA (n6*nskip*sizeof(dReal));
  dSetZero (invM,n6*nskip);
  for (i=0; i<nb; i++) {
    dReal *MM = invM+(i*6)*nskip+(i*6);
    MM[0] = body[i]->invMass;
    MM[nskip+1] = body[i]->invMass;
    MM[2*nskip+2] = body[i]->invMass;
    MM += 3*nskip+3;
    for (j=0; j<3; j++) for (k=0; k<3; k++) {
      MM[j*nskip+k] = invI[i*12+j*4+k];
    }
  }

  // assemble some body vectors: fe = external forces, v = velocities
  dReal *fe = (dReal*) ALLOCA (n6 * sizeof(dReal));
  dReal *v = (dReal*) ALLOCA (n6 * sizeof(dReal));
  dSetZero (fe,n6);
  dSetZero (v,n6);
  for (i=0; i<nb; i++) {
    for (j=0; j<3; j++) fe[i*6+j] = body[i]->facc[j];
    for (j=0; j<3; j++) fe[i*6+3+j] = body[i]->tacc[j];
    for (j=0; j<3; j++) v[i*6+j] = body[i]->lvel[j];
    for (j=0; j<3; j++) v[i*6+3+j] = body[i]->avel[j];
  }

  // this will be set to the velocity update
  dReal *vnew = (dReal*) ALLOCA (n6 * sizeof(dReal));
  dSetZero (vnew,n6);

  // if there are constraints, compute cforce
  if (m > 0) {
    // create a constraint equation right hand side vector `c', a constraint
    // force mixing vector `cfm', and LCP low and high bound vectors, and an
    // 'findex' vector.
    dReal *c = (dReal*) ALLOCA (m*sizeof(dReal));
    dReal *cfm = (dReal*) ALLOCA (m*sizeof(dReal));
    dReal *lo = (dReal*) ALLOCA (m*sizeof(dReal));
    dReal *hi = (dReal*) ALLOCA (m*sizeof(dReal));
    int *findex = (int*) alloca (m*sizeof(int));
    dSetZero (c,m);
    dSetValue (cfm,m,world->global_cfm);
    dSetValue (lo,m,-dInfinity);
    dSetValue (hi,m, dInfinity);
    for (i=0; i<m; i++) findex[i] = -1;

    // create (m,6*nb) jacobian mass matrix `J', and fill it with constraint
    // data. also fill the c vector.
#   ifdef TIMING
    dTimerNow ("create J");
#   endif
    dReal *J = (dReal*) ALLOCA (m*nskip*sizeof(dReal));
    dSetZero (J,m*nskip);
    dxJoint::Info2 Jinfo;
    Jinfo.rowskip = nskip;
    Jinfo.fps = dRecip(stepsize);
    Jinfo.erp = world->global_erp;
    for (i=0; i<nj; i++) {
      Jinfo.J1l = J + nskip*ofs[i] + 6*joint[i]->node[0].body->tag;
      Jinfo.J1a = Jinfo.J1l + 3;
      if (joint[i]->node[1].body) {
	Jinfo.J2l = J + nskip*ofs[i] + 6*joint[i]->node[1].body->tag;
	Jinfo.J2a = Jinfo.J2l + 3;
      }
      else {
	Jinfo.J2l = 0;
	Jinfo.J2a = 0;
      }
      Jinfo.c = c + ofs[i];
      Jinfo.cfm = cfm + ofs[i];
      Jinfo.lo = lo + ofs[i];
      Jinfo.hi = hi + ofs[i];
      Jinfo.findex = findex + ofs[i];
      joint[i]->vtable->getInfo2 (joint[i],&Jinfo);
      // adjust returned findex values for global index numbering
      for (j=0; j<info[i].m; j++) {
	if (findex[ofs[i] + j] >= 0) findex[ofs[i] + j] += ofs[i];
      }
    }

    // compute A = J*invM*J'
#   ifdef TIMING
    dTimerNow ("compute A");
#   endif
    dReal *JinvM = (dReal*) ALLOCA (m*nskip*sizeof(dReal));
    dSetZero (JinvM,m*nskip);
    dMultiply0 (JinvM,J,invM,m,n6,n6);
    int mskip = dPAD(m);
    dReal *A = (dReal*) ALLOCA (m*mskip*sizeof(dReal));
    dSetZero (A,m*mskip);
    dMultiply2 (A,JinvM,J,m,n6,m);

    // add cfm to the diagonal of A
    for (i=0; i<m; i++) A[i*mskip+i] += cfm[i] * Jinfo.fps;

#   ifdef COMPARE_METHODS
    comparator.nextMatrix (A,m,m,1,"A");
#   endif

    // compute `rhs', the right hand side of the equation J*a=c
#   ifdef TIMING
    dTimerNow ("compute rhs");
#   endif
    dReal *tmp1 = (dReal*) ALLOCA (n6 * sizeof(dReal));
    dSetZero (tmp1,n6);
    dMultiply0 (tmp1,invM,fe,n6,n6,1);
    for (i=0; i<n6; i++) tmp1[i] += v[i]/stepsize;
    dReal *rhs = (dReal*) ALLOCA (m * sizeof(dReal));
    dSetZero (rhs,m);
    dMultiply0 (rhs,J,tmp1,m,n6,1);
    for (i=0; i<m; i++) rhs[i] = c[i]/stepsize - rhs[i];

#   ifdef COMPARE_METHODS
    comparator.nextMatrix (c,m,1,0,"c");
    comparator.nextMatrix (rhs,m,1,0,"rhs");
#   endif

    // solve the LCP problem and get lambda.
    // this will destroy A but that's okay
#   ifdef TIMING
    dTimerNow ("solving LCP problem");
#   endif
    dReal *lambda = (dReal*) ALLOCA (m * sizeof(dReal));
    dReal *residual = (dReal*) ALLOCA (m * sizeof(dReal));
    dSolveLCP (m,A,lambda,rhs,residual,nub,lo,hi,findex);

//  OLD WAY - direct factor and solve
//
//    // factorize A (L*L'=A)
//#   ifdef TIMING
//    dTimerNow ("factorize A");
//#   endif
//    dReal *L = (dReal*) ALLOCA (m*mskip*sizeof(dReal));
//    memcpy (L,A,m*mskip*sizeof(dReal));
//    if (dFactorCholesky (L,m)==0) dDebug (0,"A is not positive definite");
//
//    // compute lambda
//#   ifdef TIMING
//    dTimerNow ("compute lambda");
//#   endif
//    dReal *lambda = (dReal*) ALLOCA (m * sizeof(dReal));
//    memcpy (lambda,rhs,m * sizeof(dReal));
//    dSolveCholesky (L,lambda,m);

#   ifdef COMPARE_METHODS
    comparator.nextMatrix (lambda,m,1,0,"lambda");
#   endif

    // compute the velocity update `vnew'
#   ifdef TIMING
    dTimerNow ("compute velocity update");
#   endif
    dMultiply1 (tmp1,J,lambda,n6,m,1);
    for (i=0; i<n6; i++) tmp1[i] += fe[i];
    dMultiply0 (vnew,invM,tmp1,n6,n6,1);
    for (i=0; i<n6; i++) vnew[i] = v[i] + stepsize*vnew[i];

    // see if the constraint has worked: compute J*vnew and make sure it equals
    // `c' (to within a certain tolerance).
#   ifdef TIMING
    dTimerNow ("verify constraint equation");
#   endif
    dMultiply0 (tmp1,J,vnew,m,n6,1);
    dReal err = 0;
    for (i=0; i<m; i++) err += dFabs(tmp1[i]-c[i]);
    printf ("%.6e\n",err);
  }
  else {
    // no constraints
    dMultiply0 (vnew,invM,fe,n6,n6,1);
    for (i=0; i<n6; i++) vnew[i] = v[i] + stepsize*vnew[i];
  }

# ifdef COMPARE_METHODS
  comparator.nextMatrix (vnew,n6,1,0,"vnew");
# endif

  // apply the velocity update to the bodies
# ifdef TIMING
  dTimerNow ("update velocity");
# endif
  for (i=0; i<nb; i++) {
    for (j=0; j<3; j++) body[i]->lvel[j] = vnew[i*6+j];
    for (j=0; j<3; j++) body[i]->avel[j] = vnew[i*6+3+j];
  }

  // update the position and orientation from the new linear/angular velocity
  // (over the given timestep)
# ifdef TIMING
  dTimerNow ("update position");
# endif
  for (i=0; i<nb; i++) moveAndRotateBody (body[i],stepsize);

# ifdef TIMING
  dTimerNow ("tidy up");
# endif

  // zero all force accumulators
  for (i=0; i<nb; i++) {
    body[i]->facc[0] = 0;
    body[i]->facc[1] = 0;
    body[i]->facc[2] = 0;
    body[i]->facc[3] = 0;
    body[i]->tacc[0] = 0;
    body[i]->tacc[1] = 0;
    body[i]->tacc[2] = 0;
    body[i]->tacc[3] = 0;
  }

# ifdef TIMING
  dTimerEnd();
  if (m > 0) dTimerReport (stdout,1);
# endif
}

//****************************************************************************
// an optimized version of dInternalStepIsland1()

void dInternalStepIsland_x2 (dxWorld *world, dxBody * const *body, int nb,
			     dxJoint * const *_joint, int nj, dReal stepsize)
{
  int i,j,k;
# ifdef TIMING
  dTimerStart("preprocessing");
# endif

  dReal stepsize1 = dRecip(stepsize);

  // number all bodies in the body list - set their tag values
  for (i=0; i<nb; i++) body[i]->tag = i;

  // make a local copy of the joint array, because we might want to modify it.
  // (the "dxJoint *const*" declaration says we're allowed to modify the joints
  // but not the joint array, because the caller might need it unchanged).
  dxJoint **joint = (dxJoint**) ALLOCA (nj * sizeof(dxJoint*));
  memcpy (joint,_joint,nj * sizeof(dxJoint*));

  // for all bodies, compute the inertia tensor and its inverse in the global
  // frame, and compute the rotational force and add it to the torque
  // accumulator. I and invI are vertically stacked 3x4 matrices, one per body.
  // @@@ check computation of rotational force.
  dReal *I = (dReal*) ALLOCA (3*nb*4 * sizeof(dReal));
  dReal *invI = (dReal*) ALLOCA (3*nb*4 * sizeof(dReal));
  dSetZero (I,3*nb*4);
  dSetZero (invI,3*nb*4);
  for (i=0; i<nb; i++) {
    dReal tmp[12];
    // compute inertia tensor in global frame
    dMULTIPLY2_333 (tmp,body[i]->mass.I,body[i]->R);
    dMULTIPLY0_333 (I+i*12,body[i]->R,tmp);
    // compute inverse inertia tensor in global frame
    dMULTIPLY2_333 (tmp,body[i]->invI,body[i]->R);
    dMULTIPLY0_333 (invI+i*12,body[i]->R,tmp);
    // compute rotational force
    dMULTIPLY0_331 (tmp,I+i*12,body[i]->avel);
    dCROSS (body[i]->tacc,-=,body[i]->avel,tmp);
  }

  // add the gravity force to all bodies
  for (i=0; i<nb; i++) {
    if ((body[i]->flags & dxBodyNoGravity)==0) {
      body[i]->facc[0] += body[i]->mass.mass * world->gravity[0];
      body[i]->facc[1] += body[i]->mass.mass * world->gravity[1];
      body[i]->facc[2] += body[i]->mass.mass * world->gravity[2];
    }
  }

  // get m = total constraint dimension, nub = number of unbounded variables.
  // create constraint offset array and number-of-rows array for all joints.
  // the constraints are re-ordered as follows: the purely unbounded
  // constraints, the mixed unbounded + LCP constraints, and last the purely
  // LCP constraints. this assists the LCP solver to put all unbounded
  // variables at the start for a quick factorization.
  //
  // joints with m=0 are inactive and are removed from the joints array
  // entirely, so that the code that follows does not consider them.
  // also number all active joints in the joint list (set their tag values).
  // inactive joints receive a tag value of -1.

  int m = 0;
  dxJoint::Info1 *info = (dxJoint::Info1*) ALLOCA (nj*sizeof(dxJoint::Info1));
  int *ofs = (int*) ALLOCA (nj*sizeof(int));
  for (i=0, j=0; j<nj; j++) {	// i=dest, j=src
    joint[j]->vtable->getInfo1 (joint[j],info+i);
    dIASSERT (info[i].m >= 0 && info[i].m <= 6 &&
	      info[i].nub >= 0 && info[i].nub <= info[i].m);
    if (info[i].m > 0) {
      joint[i] = joint[j];
      joint[i]->tag = i;
      i++;
    }
    else {
      joint[j]->tag = -1;
    }
  }
  nj = i;

  // the purely unbounded constraints
  for (i=0; i<nj; i++) if (info[i].nub == info[i].m) {
    ofs[i] = m;
    m += info[i].m;
  }
  int nub = m;
  // the mixed unbounded + LCP constraints
  for (i=0; i<nj; i++) if (info[i].nub > 0 && info[i].nub < info[i].m) {
    ofs[i] = m;
    m += info[i].m;
  }
  // the purely LCP constraints
  for (i=0; i<nj; i++) if (info[i].nub == 0) {
    ofs[i] = m;
    m += info[i].m;
  }

  // this will be set to the force due to the constraints
  dReal *cforce = (dReal*) ALLOCA (nb*8 * sizeof(dReal));
  dSetZero (cforce,nb*8);

  // if there are constraints, compute cforce
  if (m > 0) {
    // create a constraint equation right hand side vector `c', a constraint
    // force mixing vector `cfm', and LCP low and high bound vectors, and an
    // 'findex' vector.
    dReal *c = (dReal*) ALLOCA (m*sizeof(dReal));
    dReal *cfm = (dReal*) ALLOCA (m*sizeof(dReal));
    dReal *lo = (dReal*) ALLOCA (m*sizeof(dReal));
    dReal *hi = (dReal*) ALLOCA (m*sizeof(dReal));
    int *findex = (int*) alloca (m*sizeof(int));
    dSetZero (c,m);
    dSetValue (cfm,m,world->global_cfm);
    dSetValue (lo,m,-dInfinity);
    dSetValue (hi,m, dInfinity);
    for (i=0; i<m; i++) findex[i] = -1;

    // get jacobian data from constraints. a (2*m)x8 matrix will be created
    // to store the two jacobian blocks from each constraint. it has this
    // format:
    //
    //   l l l 0 a a a 0  \ 
    //   l l l 0 a a a 0   }-- jacobian body 1 block for joint 0 (3 rows)
    //   l l l 0 a a a 0  /
    //   l l l 0 a a a 0  \ 
    //   l l l 0 a a a 0   }-- jacobian body 2 block for joint 0 (3 rows)
    //   l l l 0 a a a 0  /
    //   l l l 0 a a a 0  }--- jacobian body 1 block for joint 1 (1 row)
    //   l l l 0 a a a 0  }--- jacobian body 2 block for joint 1 (1 row)
    //   etc...
    //
    //   (lll) = linear jacobian data
    //   (aaa) = angular jacobian data
    //
#   ifdef TIMING
    dTimerNow ("create J");
#   endif
    dReal *J = (dReal*) ALLOCA (2*m*8*sizeof(dReal));
    dSetZero (J,2*m*8);
    dxJoint::Info2 Jinfo;
    Jinfo.rowskip = 8;
    Jinfo.fps = stepsize1;
    Jinfo.erp = world->global_erp;
    for (i=0; i<nj; i++) {
      Jinfo.J1l = J + 2*8*ofs[i];
      Jinfo.J1a = Jinfo.J1l + 4;
      Jinfo.J2l = Jinfo.J1l + 8*info[i].m;
      Jinfo.J2a = Jinfo.J2l + 4;
      Jinfo.c = c + ofs[i];
      Jinfo.cfm = cfm + ofs[i];
      Jinfo.lo = lo + ofs[i];
      Jinfo.hi = hi + ofs[i];
      Jinfo.findex = findex + ofs[i];
      joint[i]->vtable->getInfo2 (joint[i],&Jinfo);
      // adjust returned findex values for global index numbering
      for (j=0; j<info[i].m; j++) {
	if (findex[ofs[i] + j] >= 0) findex[ofs[i] + j] += ofs[i];
      }
    }

    // compute A = J*invM*J'. first compute JinvM = J*invM. this has the same
    // format as J so we just go through the constraints in J multiplying by
    // the appropriate scalars and matrices.
#   ifdef TIMING
    dTimerNow ("compute A");
#   endif
    dReal *JinvM = (dReal*) ALLOCA (2*m*8*sizeof(dReal));
    dSetZero (JinvM,2*m*8);
    for (i=0; i<nj; i++) {
      int b = joint[i]->node[0].body->tag;
      dReal body_invMass = body[b]->invMass;
      dReal *body_invI = invI + b*12;
      dReal *Jsrc = J + 2*8*ofs[i];
      dReal *Jdst = JinvM + 2*8*ofs[i];
      for (j=info[i].m-1; j>=0; j--) {
	for (k=0; k<3; k++) Jdst[k] = Jsrc[k] * body_invMass;
	dMULTIPLY0_133 (Jdst+4,Jsrc+4,body_invI);
	Jsrc += 8;
	Jdst += 8;
      }
      if (joint[i]->node[1].body) {
	b = joint[i]->node[1].body->tag;
	body_invMass = body[b]->invMass;
	body_invI = invI + b*12;
	for (j=info[i].m-1; j>=0; j--) {
	  for (k=0; k<3; k++) Jdst[k] = Jsrc[k] * body_invMass;
	  dMULTIPLY0_133 (Jdst+4,Jsrc+4,body_invI);
	  Jsrc += 8;
	  Jdst += 8;
	}
      }
    }

    // now compute A = JinvM * J'. A's rows and columns are grouped by joint,
    // i.e. in the same way as the rows of J. block (i,j) of A is only nonzero
    // if joints i and j have at least one body in common. this fact suggests
    // the algorithm used to fill A:
    //
    //    for b = all bodies
    //      n = number of joints attached to body b
    //      for i = 1..n
    //        for j = i+1..n
    //          ii = actual joint number for i
    //          jj = actual joint number for j
    //          // (ii,jj) will be set to all pairs of joints around body b
    //          compute blockwise: A(ii,jj) += JinvM(ii) * J(jj)'
    //
    // this algorithm catches all pairs of joints that have at least one body
    // in common. it does not compute the diagonal blocks of A however -
    // another similar algorithm does that.

    int mskip = dPAD(m);
    dReal *A = (dReal*) ALLOCA (m*mskip*sizeof(dReal));
    dSetZero (A,m*mskip);
    for (i=0; i<nb; i++) {
      for (dxJointNode *n1=body[i]->firstjoint; n1; n1=n1->next) {
	for (dxJointNode *n2=n1->next; n2; n2=n2->next) {
	  // get joint numbers and ensure ofs[j1] >= ofs[j2]
	  int j1 = n1->joint->tag;
	  int j2 = n2->joint->tag;
	  if (ofs[j1] < ofs[j2]) {
	    int tmp = j1;
	    j1 = j2;
	    j2 = tmp;
	  }

	  // if either joint was tagged as -1 then it is an inactive (m=0)
	  // joint that should not be considered
	  if (j1==-1 || j2==-1) continue;

	  // determine if body i is the 1st or 2nd body of joints j1 and j2
	  int jb1 = (joint[j1]->node[1].body == body[i]);
	  int jb2 = (joint[j2]->node[1].body == body[i]);
	  // jb1/jb2 must be 0 for joints with only one body
	  dIASSERT(joint[j1]->node[1].body || jb1==0);
	  dIASSERT(joint[j2]->node[1].body || jb2==0);

	  // set block of A
	  MultiplyAdd2_p8r (A + ofs[j1]*mskip + ofs[j2],
			    JinvM + 2*8*ofs[j1] + jb1*8*info[j1].m,
			    J     + 2*8*ofs[j2] + jb2*8*info[j2].m,
			    info[j1].m,info[j2].m, mskip);
	}
      }
    }
    // compute diagonal blocks of A
    for (i=0; i<nj; i++) {
      Multiply2_p8r (A + ofs[i]*(mskip+1),
		     JinvM + 2*8*ofs[i],
		     J + 2*8*ofs[i],
		     info[i].m,info[i].m, mskip);
      if (joint[i]->node[1].body) {
	MultiplyAdd2_p8r (A + ofs[i]*(mskip+1),
			  JinvM + 2*8*ofs[i] + 8*info[i].m,
			  J + 2*8*ofs[i] + 8*info[i].m,
			  info[i].m,info[i].m, mskip);
      }
    }

    // add cfm to the diagonal of A
    for (i=0; i<m; i++) A[i*mskip+i] += cfm[i] * stepsize1;

#   ifdef COMPARE_METHODS
    comparator.nextMatrix (A,m,m,1,"A");
#   endif

    // compute the right hand side `rhs'
#   ifdef TIMING
    dTimerNow ("compute rhs");
#   endif
    dReal *tmp1 = (dReal*) ALLOCA (nb*8 * sizeof(dReal));
    dSetZero (tmp1,nb*8);
    // put v/h + invM*fe into tmp1
    for (i=0; i<nb; i++) {
      dReal body_invMass = body[i]->invMass;
      dReal *body_invI = invI + i*12;
      for (j=0; j<3; j++) tmp1[i*8+j] = body[i]->facc[j] * body_invMass +
			    body[i]->lvel[j] * stepsize1;
      dMULTIPLY0_331 (tmp1 + i*8 + 4,body_invI,body[i]->tacc);
      for (j=0; j<3; j++) tmp1[i*8+4+j] += body[i]->avel[j] * stepsize1;
    }
    // put J*tmp1 into rhs
    dReal *rhs = (dReal*) ALLOCA (m * sizeof(dReal));
    dSetZero (rhs,m);
    for (i=0; i<nj; i++) {
      dReal *JJ = J + 2*8*ofs[i];
      Multiply0_p81 (rhs+ofs[i],JJ,
		     tmp1 + 8*joint[i]->node[0].body->tag, info[i].m);
      if (joint[i]->node[1].body) {
	MultiplyAdd0_p81 (rhs+ofs[i],JJ + 8*info[i].m,
			  tmp1 + 8*joint[i]->node[1].body->tag, info[i].m);
      }
    }
    // complete rhs
    for (i=0; i<m; i++) rhs[i] = c[i]*stepsize1 - rhs[i];

#   ifdef COMPARE_METHODS
    comparator.nextMatrix (c,m,1,0,"c");
    comparator.nextMatrix (rhs,m,1,0,"rhs");
#   endif

    // solve the LCP problem and get lambda.
    // this will destroy A but that's okay
#   ifdef TIMING
    dTimerNow ("solving LCP problem");
#   endif
    dReal *lambda = (dReal*) ALLOCA (m * sizeof(dReal));
    dReal *residual = (dReal*) ALLOCA (m * sizeof(dReal));
    dSolveLCP (m,A,lambda,rhs,residual,nub,lo,hi,findex);

//  OLD WAY - direct factor and solve
//
//    // factorize A (L*L'=A)
//#   ifdef TIMING
//    dTimerNow ("factorize A");
//#   endif
//    dReal *L = (dReal*) ALLOCA (m*mskip*sizeof(dReal));
//    memcpy (L,A,m*mskip*sizeof(dReal));
//#   ifdef FAST_FACTOR
//    dFastFactorCholesky (L,m);  // does not report non positive definiteness
//#   else
//    if (dFactorCholesky (L,m)==0) dDebug (0,"A is not positive definite");
//#   endif
//
//    // compute lambda
//#   ifdef TIMING
//    dTimerNow ("compute lambda");
//#   endif
//    dReal *lambda = (dReal*) ALLOCA (m * sizeof(dReal));
//    memcpy (lambda,rhs,m * sizeof(dReal));
//    dSolveCholesky (L,lambda,m);

#   ifdef COMPARE_METHODS
    comparator.nextMatrix (lambda,m,1,0,"lambda");
#   endif

    // compute the constraint force `cforce'
#   ifdef TIMING
    dTimerNow ("compute constraint force");
#   endif
    // compute cforce = J'*lambda
    for (i=0; i<nj; i++) {
      dReal *JJ = J + 2*8*ofs[i];
      dxBody* b1 = joint[i]->node[0].body;
      dxBody* b2 = joint[i]->node[1].body;
      dJointFeedback *fb = joint[i]->feedback;

      if (fb) {
	// the user has requested feedback on the amount of force that this
	// joint is applying to the bodies. we use a slightly slower
	// computation that splits out the force components and puts them
	// in the feedback structure.
	dReal data1[8],data2[8];
	Multiply1_8q1 (data1, JJ, lambda+ofs[i], info[i].m);
	dReal *cf1 = cforce + 8*b1->tag;
	cf1[0] += (fb->f1[0] = data1[0]);
	cf1[1] += (fb->f1[1] = data1[1]);
	cf1[2] += (fb->f1[2] = data1[2]);
	cf1[4] += (fb->t1[0] = data1[4]);
	cf1[5] += (fb->t1[1] = data1[5]);
	cf1[6] += (fb->t1[2] = data1[6]);
	if (b2){
	  Multiply1_8q1 (data2, JJ + 8*info[i].m, lambda+ofs[i], info[i].m);
	  dReal *cf2 = cforce + 8*b2->tag;
	  cf2[0] += (fb->f2[0] = data2[0]);
	  cf2[1] += (fb->f2[1] = data2[1]);
	  cf2[2] += (fb->f2[2] = data2[2]);
	  cf2[4] += (fb->t2[0] = data2[4]);
	  cf2[5] += (fb->t2[1] = data2[5]);
	  cf2[6] += (fb->t2[2] = data2[6]);
	}
      }
      else {
	// no feedback is required, let's compute cforce the faster way
	MultiplyAdd1_8q1 (cforce + 8*b1->tag,JJ, lambda+ofs[i], info[i].m);
	if (b2) {
	  MultiplyAdd1_8q1 (cforce + 8*b2->tag,
			    JJ + 8*info[i].m, lambda+ofs[i], info[i].m);
	}
      }
    }
  }

  // compute the velocity update
# ifdef TIMING
  dTimerNow ("compute velocity update");
# endif

  // add fe to cforce
  for (i=0; i<nb; i++) {
    for (j=0; j<3; j++) cforce[i*8+j] += body[i]->facc[j];
    for (j=0; j<3; j++) cforce[i*8+4+j] += body[i]->tacc[j];
  }
  // multiply cforce by stepsize
  for (i=0; i < nb*8; i++) cforce[i] *= stepsize;
  // add invM * cforce to the body velocity
  for (i=0; i<nb; i++) {
    dReal body_invMass = body[i]->invMass;
    dReal *body_invI = invI + i*12;
    for (j=0; j<3; j++) body[i]->lvel[j] += body_invMass * cforce[i*8+j];
    dMULTIPLYADD0_331 (body[i]->avel,body_invI,cforce+i*8+4);
  }

  // update the position and orientation from the new linear/angular velocity
  // (over the given timestep)
# ifdef TIMING
  dTimerNow ("update position");
# endif
  for (i=0; i<nb; i++) moveAndRotateBody (body[i],stepsize);

# ifdef COMPARE_METHODS
  dReal *tmp_vnew = (dReal*) ALLOCA (nb*6*sizeof(dReal));
  for (i=0; i<nb; i++) {
    for (j=0; j<3; j++) tmp_vnew[i*6+j] = body[i]->lvel[j];
    for (j=0; j<3; j++) tmp_vnew[i*6+3+j] = body[i]->avel[j];
  }
  comparator.nextMatrix (tmp_vnew,nb*6,1,0,"vnew");
# endif

# ifdef TIMING
  dTimerNow ("tidy up");
# endif

  // zero all force accumulators
  for (i=0; i<nb; i++) {
    body[i]->facc[0] = 0;
    body[i]->facc[1] = 0;
    body[i]->facc[2] = 0;
    body[i]->facc[3] = 0;
    body[i]->tacc[0] = 0;
    body[i]->tacc[1] = 0;
    body[i]->tacc[2] = 0;
    body[i]->tacc[3] = 0;
  }

# ifdef TIMING
  dTimerEnd();
  if (m > 0) dTimerReport (stdout,1);
# endif
}

//****************************************************************************

void dInternalStepIsland (dxWorld *world, dxBody * const *body, int nb,
			  dxJoint * const *joint, int nj, dReal stepsize)
{
# ifndef COMPARE_METHODS
  dInternalStepIsland_x2 (world,body,nb,joint,nj,stepsize);
# endif

# ifdef COMPARE_METHODS
  int i;

  // save body state
  dxBody *state = (dxBody*) ALLOCA (nb*sizeof(dxBody));
  for (i=0; i<nb; i++) memcpy (state+i,body[i],sizeof(dxBody));

  // take slow step
  comparator.reset();
  dInternalStepIsland_x1 (world,body,nb,joint,nj,stepsize);
  comparator.end();

  // restore state
  for (i=0; i<nb; i++) memcpy (body[i],state+i,sizeof(dxBody));

  // take fast step
  dInternalStepIsland_x2 (world,body,nb,joint,nj,stepsize);
  comparator.end();

  //comparator.dump();
  //_exit (1);
# endif
}
