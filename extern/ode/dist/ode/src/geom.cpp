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

the rule is that only the low level primitive collision functions should set
dContactGeom::g1 and dContactGeom::g2.

*/

#define SHARED_GEOM_H_INCLUDED_FROM_DEFINING_FILE 1
#include <ode/common.h>
#include <ode/geom.h>
#include <ode/rotation.h>
#include <ode/odemath.h>
#include <ode/memory.h>
#include <ode/misc.h>
#include <ode/objects.h>
#include <ode/matrix.h>
#include "objects.h"
#include "array.h"
#include "geom_internal.h"

//****************************************************************************
// collision utilities.

// given a pointer `p' to a dContactGeom, return the dContactGeom at
// p + skip bytes.

#define CONTACT(p,skip) ((dContactGeom*) (((char*)p) + (skip)))


// if the spheres (p1,r1) and (p2,r2) collide, set the contact `c' and
// return 1, else return 0.

static int dCollideSpheres (dVector3 p1, dReal r1,
			    dVector3 p2, dReal r2, dContactGeom *c)
{
  // printf ("d=%.2f  (%.2f %.2f %.2f) (%.2f %.2f %.2f) r1=%.2f r2=%.2f\n",
  //	  d,p1[0],p1[1],p1[2],p2[0],p2[1],p2[2],r1,r2);

  dReal d = dDISTANCE (p1,p2);
  if (d > (r1 + r2)) return 0;
  if (d <= 0) {
    c->pos[0] = p1[0];
    c->pos[1] = p1[1];
    c->pos[2] = p1[2];
    c->normal[0] = 1;
    c->normal[1] = 0;
    c->normal[2] = 0;
    c->depth = r1 + r2;
  }
  else {
    dReal d1 = dRecip (d);
    c->normal[0] = (p1[0]-p2[0])*d1;
    c->normal[1] = (p1[1]-p2[1])*d1;
    c->normal[2] = (p1[2]-p2[2])*d1;
    dReal k = REAL(0.5) * (r2 - r1 - d);
    c->pos[0] = p1[0] + c->normal[0]*k;
    c->pos[1] = p1[1] + c->normal[1]*k;
    c->pos[2] = p1[2] + c->normal[2]*k;
    c->depth = r1 + r2 - d;
  }
  return 1;
}


// given two lines
//    qa = pa + alpha* ua
//    qb = pb + beta * ub
// where pa,pb are two points, ua,ub are two unit length vectors, and alpha,
// beta go from [-inf,inf], return alpha and beta such that qa and qb are
// as close as possible

static void lineClosestApproach (const dVector3 pa, const dVector3 ua,
				 const dVector3 pb, const dVector3 ub,
				 dReal *alpha, dReal *beta)
{
  dVector3 p;
  p[0] = pb[0] - pa[0];
  p[1] = pb[1] - pa[1];
  p[2] = pb[2] - pa[2];
  dReal uaub = dDOT(ua,ub);
  dReal q1 =  dDOT(ua,p);
  dReal q2 = -dDOT(ub,p);
  dReal d = 1-uaub*uaub;
  if (d <= 0) {
    // @@@ this needs to be made more robust
    *alpha = 0;
    *beta  = 0;
  }
  else {
    d = dRecip(d);
    *alpha = (q1 + uaub*q2)*d;
    *beta  = (uaub*q1 + q2)*d;
  }
}


// given two line segments A and B with endpoints a1-a2 and b1-b2, return the
// points on A and B that are closest to each other (in cp1 and cp2).
// in the case of parallel lines where there are multiple solutions, a
// solution involving the endpoint of at least one line will be returned.
// this will work correctly for zero length lines, e.g. if a1==a2 and/or
// b1==b2.
//
// the algorithm works by applying the voronoi clipping rule to the features
// of the line segments. the three features of each line segment are the two
// endpoints and the line between them. the voronoi clipping rule states that,
// for feature X on line A and feature Y on line B, the closest points PA and
// PB between X and Y are globally the closest points if PA is in V(Y) and
// PB is in V(X), where V(X) is the voronoi region of X.


void dClosestLineSegmentPoints (dVector3 const a1, dVector3 const a2,
				dVector3 const b1, dVector3 const b2,
				dVector3 cp1, dVector3 cp2)
{
  dVector3 a1a2,b1b2,a1b1,a1b2,a2b1,a2b2,n;
  dReal la,lb,k,da1,da2,da3,da4,db1,db2,db3,db4,det;

#define SET2(a,b) a[0]=b[0]; a[1]=b[1]; a[2]=b[2];
#define SET3(a,b,op,c) a[0]=b[0] op c[0]; a[1]=b[1] op c[1]; a[2]=b[2] op c[2];

  // check vertex-vertex features

  SET3 (a1a2,a2,-,a1);
  SET3 (b1b2,b2,-,b1);
  SET3 (a1b1,b1,-,a1);
  da1 = dDOT(a1a2,a1b1);
  db1 = dDOT(b1b2,a1b1);
  if (da1 <= 0 && db1 >= 0) {
    SET2 (cp1,a1);
    SET2 (cp2,b1);
    return;
  }

  SET3 (a1b2,b2,-,a1);
  da2 = dDOT(a1a2,a1b2);
  db2 = dDOT(b1b2,a1b2);
  if (da2 <= 0 && db2 <= 0) {
    SET2 (cp1,a1);
    SET2 (cp2,b2);
    return;
  }

  SET3 (a2b1,b1,-,a2);
  da3 = dDOT(a1a2,a2b1);
  db3 = dDOT(b1b2,a2b1);
  if (da3 >= 0 && db3 >= 0) {
    SET2 (cp1,a2);
    SET2 (cp2,b1);
    return;
  }

  SET3 (a2b2,b2,-,a2);
  da4 = dDOT(a1a2,a2b2);
  db4 = dDOT(b1b2,a2b2);
  if (da4 >= 0 && db4 <= 0) {
    SET2 (cp1,a2);
    SET2 (cp2,b2);
    return;
  }

  // check edge-vertex features.
  // if one or both of the lines has zero length, we will never get to here,
  // so we do not have to worry about the following divisions by zero.

  la = dDOT(a1a2,a1a2);
  if (da1 >= 0 && da3 <= 0) {
    k = da1 / la;
    SET3 (n,a1b1,-,k*a1a2);
    if (dDOT(b1b2,n) >= 0) {
      SET3 (cp1,a1,+,k*a1a2);
      SET2 (cp2,b1);
      return;
    }
  }

  if (da2 >= 0 && da4 <= 0) {
    k = da2 / la;
    SET3 (n,a1b2,-,k*a1a2);
    if (dDOT(b1b2,n) <= 0) {
      SET3 (cp1,a1,+,k*a1a2);
      SET2 (cp2,b2);
      return;
    }
  }

  lb = dDOT(b1b2,b1b2);
  if (db1 <= 0 && db2 >= 0) {
    k = -db1 / lb;
    SET3 (n,-a1b1,-,k*b1b2);
    if (dDOT(a1a2,n) >= 0) {
      SET2 (cp1,a1);
      SET3 (cp2,b1,+,k*b1b2);
      return;
    }
  }

  if (db3 <= 0 && db4 >= 0) {
    k = -db3 / lb;
    SET3 (n,-a2b1,-,k*b1b2);
    if (dDOT(a1a2,n) <= 0) {
      SET2 (cp1,a2);
      SET3 (cp2,b1,+,k*b1b2);
      return;
    }
  }

  // it must be edge-edge

  k = dDOT(a1a2,b1b2);
  det = la*lb - k*k;
  if (det <= 0) {
    // this should never happen, but just in case...
    SET2(cp1,a1);
    SET2(cp2,b1);
    return;
  }
  det = dRecip (det);
  dReal alpha = (lb*da1 -  k*db1) * det;
  dReal beta  = ( k*da1 - la*db1) * det;
  SET3 (cp1,a1,+,alpha*a1a2);
  SET3 (cp2,b1,+,beta*b1b2);

# undef SET2
# undef SET3
}


// given a line segment p1-p2 and a box (center 'c', rotation 'R', side length
// vector 'side'), compute the points of closest approach between the box
// and the line. return these points in 'lret' (the point on the line) and
// 'bret' (the point on the box). if the line actually penetrates the box
// then the solution is not unique, but only one solution will be returned.
// in this case the solution points will coincide.
//
// a simple root finding algorithm is used to find the value of 't' that
// satisfies:
//		d|D(t)|^2/dt = 0
// where:
//		|D(t)| = |p(t)-b(t)|
// where p(t) is a point on the line parameterized by t:
//		p(t) = p1 + t*(p2-p1)
// and b(t) is that same point clipped to the boundary of the box. in box-
// relative coordinates d|D(t)|^2/dt is the sum of three x,y,z components
// each of which looks like this:
//
//	    t_lo     /
//	      ______/    -->t
//	     /     t_hi
//	    /
//
// t_lo and t_hi are the t values where the line passes through the planes
// corresponding to the sides of the box. the algorithm computes d|D(t)|^2/dt
// in a piecewise fashion from t=0 to t=1, stopping at the point where
// d|D(t)|^2/dt crosses from negative to positive.

static void dClosestLineBoxPoints (const dVector3 p1, const dVector3 p2,
				   const dVector3 c, const dMatrix3 R,
				   const dVector3 side,
				   dVector3 lret, dVector3 bret)
{
  int i;

  // compute the start and delta of the line p1-p2 relative to the box.
  // we will do all subsequent computations in this box-relative coordinate
  // system. we have to do a translation and rotation for each point.
  dVector3 tmp,s,v;
  tmp[0] = p1[0] - c[0];
  tmp[1] = p1[1] - c[1];
  tmp[2] = p1[2] - c[2];
  dMULTIPLY1_331 (s,R,tmp);
  tmp[0] = p2[0] - p1[0];
  tmp[1] = p2[1] - p1[1];
  tmp[2] = p2[2] - p1[2];
  dMULTIPLY1_331 (v,R,tmp);

  // mirror the line so that v has all components >= 0
  dVector3 sign;
  for (i=0; i<3; i++) {
    if (v[i] < 0) {
      s[i] = -s[i];
      v[i] = -v[i];
      sign[i] = -1;
    }
    else sign[i] = 1;
  }

  // compute v^2
  dVector3 v2;
  v2[0] = v[0]*v[0];
  v2[1] = v[1]*v[1];
  v2[2] = v[2]*v[2];

  // compute the half-sides of the box
  dReal h[3];
  h[0] = REAL(0.5) * side[0];
  h[1] = REAL(0.5) * side[1];
  h[2] = REAL(0.5) * side[2];

  // region is -1,0,+1 depending on which side of the box planes each
  // coordinate is on. tanchor in the next t value at which there is a
  // transition, or the last one if there are no more.
  int region[3];
  dReal tanchor[3];

  // find the region and tanchor values for p1
  for (i=0; i<3; i++) {
    if (v[i] > 0) {
      if (s[i] < -h[i]) {
	region[i] = -1;
	tanchor[i] = (-h[i]-s[i])/v[i];
      }
      else {
	region[i] = (s[i] > h[i]);
	tanchor[i] = (h[i]-s[i])/v[i];
      }
    }
    else {
      region[i] = 0;
      tanchor[i] = 2;		// this will never be a valid tanchor
    }
  }

  // compute d|d|^2/dt for t=0. if it's >= 0 then p1 is the closest point
  dReal t=0;
  dReal dd2dt = 0;
  for (i=0; i<3; i++) dd2dt -= (region[i] ? v2[i] : 0) * tanchor[i];
  if (dd2dt >= 0) goto got_answer;

  do {
    // find the point on the line that is at the next clip plane boundary
    dReal next_t = 1;
    for (i=0; i<3; i++) {
      if (tanchor[i] > t && tanchor[i] < 1 && tanchor[i] < next_t)
        next_t = tanchor[i];
    }

    // compute d|d|^2/dt for the next t
    dReal next_dd2dt = 0;
    for (i=0; i<3; i++) {
      next_dd2dt += (region[i] ? v2[i] : 0) * (next_t - tanchor[i]);
    }

    // if the sign of d|d|^2/dt has changed, solution = the crossover point
    if (next_dd2dt >= 0) {
      dReal m = (next_dd2dt-dd2dt)/(next_t - t);
      t -= dd2dt/m;
      goto got_answer;
    }

    // advance to the next anchor point / region
    for (i=0; i<3; i++) {
      if (tanchor[i] == next_t) {
	tanchor[i] = (h[i]-s[i])/v[i];
	region[i]++;
      }
    }
    t = next_t;
    dd2dt = next_dd2dt;
  }
  while (t < 1);
  t = 1;

  got_answer:

  // compute closest point on the line
  for (i=0; i<3; i++) lret[i] = p1[i] + t*tmp[i];	// note: tmp=p2-p1

  // compute closest point on the box
  for (i=0; i<3; i++) {
    tmp[i] = sign[i] * (s[i] + t*v[i]);
    if (tmp[i] < -h[i]) tmp[i] = -h[i];
    else if (tmp[i] > h[i]) tmp[i] = h[i];
  }
  dMULTIPLY0_331 (s,R,tmp);
  for (i=0; i<3; i++) bret[i] = s[i] + c[i];
}


// given a box (R,side), `R' is the rotation matrix for the box, and `side'
// is a vector of x/y/z side lengths, return the size of the interval of the
// box projected along the given axis. if the axis has unit length then the
// return value will be the actual diameter, otherwise the result will be
// scaled by the axis length.

static inline dReal boxDiameter (const dMatrix3 R, const dVector3 side,
				 const dVector3 axis)
{
  dVector3 q;
  dMULTIPLY1_331 (q,R,axis);	// transform axis to body-relative
  return dFabs(q[0])*side[0] + dFabs(q[1])*side[1] + dFabs(q[2])*side[2];
}


// given boxes (p1,R1,side1) and (p1,R1,side1), return 1 if they intersect
// or 0 if not.

int dBoxTouchesBox (const dVector3 p1, const dMatrix3 R1,
		    const dVector3 side1, const dVector3 p2,
		    const dMatrix3 R2, const dVector3 side2)
{
  // two boxes are disjoint if (and only if) there is a separating axis
  // perpendicular to a face from one box or perpendicular to an edge from
  // either box. the following tests are derived from:
  //    "OBB Tree: A Hierarchical Structure for Rapid Interference Detection",
  //    S.Gottschalk, M.C.Lin, D.Manocha., Proc of ACM Siggraph 1996.

  // Rij is R1'*R2, i.e. the relative rotation between R1 and R2.
  // Qij is abs(Rij)
  dVector3 p,pp;
  dReal A1,A2,A3,B1,B2,B3,R11,R12,R13,R21,R22,R23,R31,R32,R33,
    Q11,Q12,Q13,Q21,Q22,Q23,Q31,Q32,Q33;

  // get vector from centers of box 1 to box 2, relative to box 1
  p[0] = p2[0] - p1[0];
  p[1] = p2[1] - p1[1];
  p[2] = p2[2] - p1[2];
  dMULTIPLY1_331 (pp,R1,p);		// get pp = p relative to body 1

  // get side lengths / 2
  A1 = side1[0]*REAL(0.5); A2 = side1[1]*REAL(0.5); A3 = side1[2]*REAL(0.5);
  B1 = side2[0]*REAL(0.5); B2 = side2[1]*REAL(0.5); B3 = side2[2]*REAL(0.5);

  // for the following tests, excluding computation of Rij, in the worst case,
  // 15 compares, 60 adds, 81 multiplies, and 24 absolutes.
  // notation: R1=[u1 u2 u3], R2=[v1 v2 v3]

  // separating axis = u1,u2,u3
  R11 = dDOT44(R1+0,R2+0); R12 = dDOT44(R1+0,R2+1); R13 = dDOT44(R1+0,R2+2);
  Q11 = dFabs(R11); Q12 = dFabs(R12); Q13 = dFabs(R13);
  if (dFabs(pp[0]) > (A1 + B1*Q11 + B2*Q12 + B3*Q13)) return 0;
  R21 = dDOT44(R1+1,R2+0); R22 = dDOT44(R1+1,R2+1); R23 = dDOT44(R1+1,R2+2);
  Q21 = dFabs(R21); Q22 = dFabs(R22); Q23 = dFabs(R23);
  if (dFabs(pp[1]) > (A2 + B1*Q21 + B2*Q22 + B3*Q23)) return 0;
  R31 = dDOT44(R1+2,R2+0); R32 = dDOT44(R1+2,R2+1); R33 = dDOT44(R1+2,R2+2);
  Q31 = dFabs(R31); Q32 = dFabs(R32); Q33 = dFabs(R33);
  if (dFabs(pp[2]) > (A3 + B1*Q31 + B2*Q32 + B3*Q33)) return 0;

  // separating axis = v1,v2,v3
  if (dFabs(dDOT41(R2+0,p)) > (A1*Q11 + A2*Q21 + A3*Q31 + B1)) return 0;
  if (dFabs(dDOT41(R2+1,p)) > (A1*Q12 + A2*Q22 + A3*Q32 + B2)) return 0;
  if (dFabs(dDOT41(R2+2,p)) > (A1*Q13 + A2*Q23 + A3*Q33 + B3)) return 0;

  // separating axis = u1 x (v1,v2,v3)
  if (dFabs(pp[2]*R21-pp[1]*R31) > A2*Q31 + A3*Q21 + B2*Q13 + B3*Q12) return 0;
  if (dFabs(pp[2]*R22-pp[1]*R32) > A2*Q32 + A3*Q22 + B1*Q13 + B3*Q11) return 0;
  if (dFabs(pp[2]*R23-pp[1]*R33) > A2*Q33 + A3*Q23 + B1*Q12 + B2*Q11) return 0;

  // separating axis = u2 x (v1,v2,v3)
  if (dFabs(pp[0]*R31-pp[2]*R11) > A1*Q31 + A3*Q11 + B2*Q23 + B3*Q22) return 0;
  if (dFabs(pp[0]*R32-pp[2]*R12) > A1*Q32 + A3*Q12 + B1*Q23 + B3*Q21) return 0;
  if (dFabs(pp[0]*R33-pp[2]*R13) > A1*Q33 + A3*Q13 + B1*Q22 + B2*Q21) return 0;

  // separating axis = u3 x (v1,v2,v3)
  if (dFabs(pp[1]*R11-pp[0]*R21) > A1*Q21 + A2*Q11 + B2*Q33 + B3*Q32) return 0;
  if (dFabs(pp[1]*R12-pp[0]*R22) > A1*Q22 + A2*Q12 + B1*Q33 + B3*Q31) return 0;
  if (dFabs(pp[1]*R13-pp[0]*R23) > A1*Q23 + A2*Q13 + B1*Q32 + B2*Q31) return 0;

  return 1;
}


// given two boxes (p1,R1,side1) and (p2,R2,side2), collide them together and
// generate contact points. this returns 0 if there is no contact otherwise
// it returns the number of contacts generated.
// `normal' returns the contact normal.
// `depth' returns the maximum penetration depth along that normal.
// `code' returns a number indicating the type of contact that was detected:
//        1,2,3 = box 2 intersects with a face of box 1
//        4,5,6 = box 1 intersects with a face of box 2
//        7..15 = edge-edge contact
// `maxc' is the maximum number of contacts allowed to be generated, i.e.
// the size of the `contact' array.
// `contact' and `skip' are the contact array information provided to the
// collision functions. this function only fills in the position and depth
// fields.
//
// @@@ some stuff to optimize here, reuse code in contact point calculations.

extern "C" int dBoxBox (const dVector3 p1, const dMatrix3 R1,
			const dVector3 side1, const dVector3 p2,
			const dMatrix3 R2, const dVector3 side2,
			dVector3 normal, dReal *depth, int *code,
			int maxc, dContactGeom *contact, int skip)
{
  dVector3 p,pp,normalC;
  const dReal *normalR = 0;
  dReal A1,A2,A3,B1,B2,B3,R11,R12,R13,R21,R22,R23,R31,R32,R33,
    Q11,Q12,Q13,Q21,Q22,Q23,Q31,Q32,Q33,s,s2,l;
  int i,invert_normal;

  // get vector from centers of box 1 to box 2, relative to box 1
  p[0] = p2[0] - p1[0];
  p[1] = p2[1] - p1[1];
  p[2] = p2[2] - p1[2];
  dMULTIPLY1_331 (pp,R1,p);		// get pp = p relative to body 1

  // get side lengths / 2
  A1 = side1[0]*REAL(0.5); A2 = side1[1]*REAL(0.5); A3 = side1[2]*REAL(0.5);
  B1 = side2[0]*REAL(0.5); B2 = side2[1]*REAL(0.5); B3 = side2[2]*REAL(0.5);

  // Rij is R1'*R2, i.e. the relative rotation between R1 and R2
  R11 = dDOT44(R1+0,R2+0); R12 = dDOT44(R1+0,R2+1); R13 = dDOT44(R1+0,R2+2);
  R21 = dDOT44(R1+1,R2+0); R22 = dDOT44(R1+1,R2+1); R23 = dDOT44(R1+1,R2+2);
  R31 = dDOT44(R1+2,R2+0); R32 = dDOT44(R1+2,R2+1); R33 = dDOT44(R1+2,R2+2);

  Q11 = dFabs(R11); Q12 = dFabs(R12); Q13 = dFabs(R13);
  Q21 = dFabs(R21); Q22 = dFabs(R22); Q23 = dFabs(R23);
  Q31 = dFabs(R31); Q32 = dFabs(R32); Q33 = dFabs(R33);

  // for all 15 possible separating axes:
  //   * see if the axis separates the boxes. if so, return 0.
  //   * find the depth of the penetration along the separating axis (s2)
  //   * if this is the largest depth so far, record it.
  // the normal vector will be set to the separating axis with the smallest
  // depth. note: normalR is set to point to a column of R1 or R2 if that is
  // the smallest depth normal so far. otherwise normalR is 0 and normalC is
  // set to a vector relative to body 1. invert_normal is 1 if the sign of
  // the normal should be flipped.

#define TEST(expr1,expr2,norm,cc) \
  s2 = dFabs(expr1) - (expr2); \
  if (s2 > 0) return 0; \
  if (s2 > s) { \
    s = s2; \
    normalR = norm; \
    invert_normal = ((expr1) < 0); \
    *code = (cc); \
  }

  s = -dInfinity;
  invert_normal = 0;
  *code = 0;

  // separating axis = u1,u2,u3
  TEST (pp[0],(A1 + B1*Q11 + B2*Q12 + B3*Q13),R1+0,1);
  TEST (pp[1],(A2 + B1*Q21 + B2*Q22 + B3*Q23),R1+1,2);
  TEST (pp[2],(A3 + B1*Q31 + B2*Q32 + B3*Q33),R1+2,3);

  // separating axis = v1,v2,v3
  TEST (dDOT41(R2+0,p),(A1*Q11 + A2*Q21 + A3*Q31 + B1),R2+0,4);
  TEST (dDOT41(R2+1,p),(A1*Q12 + A2*Q22 + A3*Q32 + B2),R2+1,5);
  TEST (dDOT41(R2+2,p),(A1*Q13 + A2*Q23 + A3*Q33 + B3),R2+2,6);

  // note: cross product axes need to be scaled when s is computed.
  // normal (n1,n2,n3) is relative to box 1.
#undef TEST
#define TEST(expr1,expr2,n1,n2,n3,cc) \
  s2 = dFabs(expr1) - (expr2); \
  if (s2 > 0) return 0; \
  l = dSqrt ((n1)*(n1) + (n2)*(n2) + (n3)*(n3)); \
  if (l > 0) { \
    s2 /= l; \
    if (s2 > s) { \
      s = s2; \
      normalR = 0; \
      normalC[0] = (n1)/l; normalC[1] = (n2)/l; normalC[2] = (n3)/l; \
      invert_normal = ((expr1) < 0); \
      *code = (cc); \
    } \
  }

  // separating axis = u1 x (v1,v2,v3)
  TEST(pp[2]*R21-pp[1]*R31,(A2*Q31+A3*Q21+B2*Q13+B3*Q12),0,-R31,R21,7);
  TEST(pp[2]*R22-pp[1]*R32,(A2*Q32+A3*Q22+B1*Q13+B3*Q11),0,-R32,R22,8);
  TEST(pp[2]*R23-pp[1]*R33,(A2*Q33+A3*Q23+B1*Q12+B2*Q11),0,-R33,R23,9);

  // separating axis = u2 x (v1,v2,v3)
  TEST(pp[0]*R31-pp[2]*R11,(A1*Q31+A3*Q11+B2*Q23+B3*Q22),R31,0,-R11,10);
  TEST(pp[0]*R32-pp[2]*R12,(A1*Q32+A3*Q12+B1*Q23+B3*Q21),R32,0,-R12,11);
  TEST(pp[0]*R33-pp[2]*R13,(A1*Q33+A3*Q13+B1*Q22+B2*Q21),R33,0,-R13,12);

  // separating axis = u3 x (v1,v2,v3)
  TEST(pp[1]*R11-pp[0]*R21,(A1*Q21+A2*Q11+B2*Q33+B3*Q32),-R21,R11,0,13);
  TEST(pp[1]*R12-pp[0]*R22,(A1*Q22+A2*Q12+B1*Q33+B3*Q31),-R22,R12,0,14);
  TEST(pp[1]*R13-pp[0]*R23,(A1*Q23+A2*Q13+B1*Q32+B2*Q31),-R23,R13,0,15);

#undef TEST

  // if we get to this point, the boxes interpenetrate. compute the normal
  // in global coordinates.
  if (normalR) {
    normal[0] = normalR[0];
    normal[1] = normalR[4];
    normal[2] = normalR[8];
  }
  else {
    dMULTIPLY0_331 (normal,R1,normalC);
  }
  if (invert_normal) {
    normal[0] = -normal[0];
    normal[1] = -normal[1];
    normal[2] = -normal[2];
  }
  *depth = -s;

  // compute contact point(s)

  if (*code > 6) {
    // an edge from box 1 touches an edge from box 2.
    // find a point pa on the intersecting edge of box 1
    dVector3 pa;
    dReal sign;
    for (i=0; i<3; i++) pa[i] = p1[i];
    sign = (dDOT14(normal,R1+0) > 0) ? REAL(1.0) : REAL(-1.0);
    for (i=0; i<3; i++) pa[i] += sign * A1 * R1[i*4];
    sign = (dDOT14(normal,R1+1) > 0) ? REAL(1.0) : REAL(-1.0);
    for (i=0; i<3; i++) pa[i] += sign * A2 * R1[i*4+1];
    sign = (dDOT14(normal,R1+2) > 0) ? REAL(1.0) : REAL(-1.0);
    for (i=0; i<3; i++) pa[i] += sign * A3 * R1[i*4+2];

    // find a point pb on the intersecting edge of box 2
    dVector3 pb;
    for (i=0; i<3; i++) pb[i] = p2[i];
    sign = (dDOT14(normal,R2+0) > 0) ? REAL(-1.0) : REAL(1.0);
    for (i=0; i<3; i++) pb[i] += sign * B1 * R2[i*4];
    sign = (dDOT14(normal,R2+1) > 0) ? REAL(-1.0) : REAL(1.0);
    for (i=0; i<3; i++) pb[i] += sign * B2 * R2[i*4+1];
    sign = (dDOT14(normal,R2+2) > 0) ? REAL(-1.0) : REAL(1.0);
    for (i=0; i<3; i++) pb[i] += sign * B3 * R2[i*4+2];

    dReal alpha,beta;
    dVector3 ua,ub;
    for (i=0; i<3; i++) ua[i] = R1[((*code)-7)/3 + i*4];
    for (i=0; i<3; i++) ub[i] = R2[((*code)-7)%3 + i*4];

    lineClosestApproach (pa,ua,pb,ub,&alpha,&beta);
    for (i=0; i<3; i++) pa[i] += ua[i]*alpha;
    for (i=0; i<3; i++) pb[i] += ub[i]*beta;

    for (i=0; i<3; i++) contact[0].pos[i] = REAL(0.5)*(pa[i]+pb[i]);
    contact[0].depth = *depth;
    return 1;
  }

  // okay, we have a face-something intersection (because the separating
  // axis is perpendicular to a face).

  // @@@ temporary: make deepest vertex on the "other" box the contact point.
  // @@@ this kind of works, but we need multiple contact points for stability,
  // @@@ especially for face-face contact.

  dVector3 vertex;
  if (*code <= 3) {
    // face from box 1 touches a vertex/edge/face from box 2.
    dReal sign;
    for (i=0; i<3; i++) vertex[i] = p2[i];
    sign = (dDOT14(normal,R2+0) > 0) ? REAL(-1.0) : REAL(1.0);
    for (i=0; i<3; i++) vertex[i] += sign * B1 * R2[i*4];
    sign = (dDOT14(normal,R2+1) > 0) ? REAL(-1.0) : REAL(1.0);
    for (i=0; i<3; i++) vertex[i] += sign * B2 * R2[i*4+1];
    sign = (dDOT14(normal,R2+2) > 0) ? REAL(-1.0) : REAL(1.0);
    for (i=0; i<3; i++) vertex[i] += sign * B3 * R2[i*4+2];
  }
  else {
    // face from box 2 touches a vertex/edge/face from box 1.
    dReal sign;
    for (i=0; i<3; i++) vertex[i] = p1[i];
    sign = (dDOT14(normal,R1+0) > 0) ? REAL(1.0) : REAL(-1.0);
    for (i=0; i<3; i++) vertex[i] += sign * A1 * R1[i*4];
    sign = (dDOT14(normal,R1+1) > 0) ? REAL(1.0) : REAL(-1.0);
    for (i=0; i<3; i++) vertex[i] += sign * A2 * R1[i*4+1];
    sign = (dDOT14(normal,R1+2) > 0) ? REAL(1.0) : REAL(-1.0);
    for (i=0; i<3; i++) vertex[i] += sign * A3 * R1[i*4+2];
  }
  for (i=0; i<3; i++) contact[0].pos[i] = vertex[i];
  contact[0].depth = *depth;
  return 1;
}

//****************************************************************************
// general support for geometry objects and classes

struct dColliderEntry {
  dColliderFn *fn;	// collider function
  int mode;		// 1 = reverse o1 and o2, 2 = no function available
};

static dArray<dxGeomClass*> *classes=0;

// function pointers and modes for n^2 class collider functions. this is an
// n*n matrix stored by row. the functions pointers are extracted from the
// class get-collider-function function.
static dArray<dColliderEntry> *colliders=0;


static inline void initCollisionArrays()
{
  if (classes==0) {
    // old way:
    //   classes = (dArray<dxGeomClass*> *) dAllocNoFree (sizeof(dArrayBase));
    //   classes->constructor();
    classes = new dArray<dxGeomClass*>;
    classes->setSize (1);	// force allocation of array data memory
    dAllocDontReport (classes);
    dAllocDontReport (classes->data());
    classes->setSize (0);
  }
  if (colliders==0) {
    // old way:
    //   colliders=(dArray<dColliderEntry> *)dAllocNoFree (sizeof(dArrayBase));
    //   colliders->constructor();
    colliders = new dArray<dColliderEntry>;
    colliders->setSize (1);	// force allocation of array data memory
    dAllocDontReport (colliders);
    dAllocDontReport (colliders->data());
    colliders->setSize (0);
  }
}


int dCreateGeomClass (const dGeomClass *c)
{
  dUASSERT(c && c->bytes >= 0 && c->collider && c->aabb,"bad geom class");
  initCollisionArrays();

  int n = classes->size();
  dxGeomClass *gc = (dxGeomClass*) dAlloc (sizeof(dxGeomClass));
  dAllocDontReport (gc);
  gc->collider = c->collider;
  gc->aabb = c->aabb;
  gc->aabb_test = c->aabb_test;
  gc->dtor = c->dtor;
  gc->num = n;
  gc->size = SIZEOF_DXGEOM + c->bytes;
  classes->push (gc);

  // make room for n^2 class collider function pointers - these entries will
  // be filled as dCollide() is called.
  colliders->setSize ((n+1)*(n+1));
  memset (colliders->data(),0,(n+1)*(n+1)*sizeof(dColliderEntry));

  return n;
}


int dCollide (dxGeom *o1, dxGeom *o2, int flags, dContactGeom *contact,
	      int skip)
{
  int i,c1,c2,a1,a2,count,swap;
  dColliderFn *fn;
  dAASSERT(o1 && o2 && contact);
  dUASSERT(classes && colliders,"no registered geometry classes");

  // no contacts if both geoms on the same body, and the body is not 0
  if (o1->body == o2->body && o1->body) return 0;

  dColliderEntry *colliders2 = colliders->data();
  c1 = o1->_class->num;
  c2 = o2->_class->num;
  a1 = c1 * classes->size() + c2;	// address 1 in collider array
  a2 = c2 * classes->size() + c1;	// address 2 in collider array
  swap = 0;		// set to 1 to swap normals before returning

  // return if there are no collider functions available
  if ((colliders2[a1].mode==2) || (colliders2[a2].mode==2)) return 0;

  if ((fn = colliders2[a1].fn)) {
    swap = colliders2[a1].mode;
    if (swap) count = (*fn) (o2,o1,flags,contact,skip);
    else count = (*fn) (o1,o2,flags,contact,skip);
  }
  else if ((fn = (*classes)[c1]->collider (c2))) {
    colliders2 [a2].fn = fn;
    colliders2 [a2].mode = 1;
    colliders2 [a1].fn = fn;	// do mode=0 assignment second so that
    colliders2 [a1].mode = 0;	// diagonal entries will have mode 0
    count = (*fn) (o1,o2,flags,contact,skip);
    swap = 0;
  }
  else if ((fn = (*classes)[c2]->collider (c1))) {
    colliders2 [a1].fn = fn;
    colliders2 [a1].mode = 1;
    colliders2 [a2].fn = fn;	// do mode=0 assignment second so that
    colliders2 [a2].mode = 0;	// diagonal entries will have mode 0
    count = (*fn) (o2,o1,flags,contact,skip);
    swap = 1;
  }
  else {
    colliders2[a1].mode = 2;
    colliders2[a2].mode = 2;
    return 0;
  }

  if (swap) {
    for (i=0; i<count; i++) {
      dContactGeom *c = CONTACT(contact,skip*i);
      c->normal[0] = -c->normal[0];
      c->normal[1] = -c->normal[1];
      c->normal[2] = -c->normal[2];
      dxGeom *tmp = c->g1;
      c->g1 = c->g2;
      c->g2 = tmp;
    }
  }

  return count;
}


int dGeomGetClass (dxGeom *g)
{
  dAASSERT (g);
  return g->_class->num;
}


void dGeomSetData (dxGeom *g, void *data)
{
  dAASSERT (g);
  g->data = data;
}


void *dGeomGetData (dxGeom *g)
{
  dAASSERT (g);
  return g->data;
}


void dGeomSetBody (dxGeom *g, dBodyID b)
{
  dAASSERT (g);
  if (b) {
    if (!g->body) dFree (g->pos,sizeof(dxPosR));
    g->body = b;
    g->pos = b->pos;
    g->R = b->R;
  }
  else {
    if (g->body) {
      dxPosR *pr = (dxPosR*) dAlloc (sizeof(dxPosR));
      g->pos = pr->pos;
      g->R = pr->R;
      memcpy (g->pos,g->body->pos,sizeof(g->pos));
      memcpy (g->R,g->body->R,sizeof(g->R));
      g->body = 0;
    }
  }
}


dBodyID dGeomGetBody (dxGeom *g)
{
  dAASSERT (g);
  return g->body;
}


void dGeomSetPosition (dxGeom *g, dReal x, dReal y, dReal z)
{
  dAASSERT (g);
  if (g->body) dBodySetPosition (g->body,x,y,z);
  else {
    g->pos[0] = x;
    g->pos[1] = y;
    g->pos[2] = z;
  }
}


void dGeomSetRotation (dxGeom *g, const dMatrix3 R)
{
  dAASSERT (g);
  if (g->body) dBodySetRotation (g->body,R);
  else memcpy (g->R,R,sizeof(dMatrix3));
}


const dReal * dGeomGetPosition (dxGeom *g)
{
  dAASSERT (g);
  return g->pos;
}


const dReal * dGeomGetRotation (dxGeom *g)
{
  dAASSERT (g);
  return g->R;
}


// for external use only. use the CLASSDATA macro inside ODE.

void * dGeomGetClassData (dxGeom *g)
{
  dAASSERT (g);
  return (void*) CLASSDATA(g);
}


dxGeom * dCreateGeom (int classnum)
{
  dUASSERT (classes && colliders && classnum >= 0 &&
	    classnum < classes->size(),"bad class number");
  int size = (*classes)[classnum]->size;
  dxGeom *geom = (dxGeom*) dAlloc (size);
  memset (geom,0,size);		// everything is initially zeroed

  geom->_class = (*classes)[classnum];
  geom->data = 0;
  geom->body = 0;

  dxPosR *pr = (dxPosR*) dAlloc (sizeof(dxPosR));
  geom->pos = pr->pos;
  geom->R = pr->R;
  dSetZero (geom->pos,4);
  dRSetIdentity (geom->R);

  return geom;
}


void dGeomDestroy (dxGeom *g)
{
  dAASSERT (g);
  if (g->spaceid) dSpaceRemove (g->spaceid,g);
  if (g->_class->dtor) g->_class->dtor (g);
  if (!g->body) dFree (g->pos,sizeof(dxPosR));
  dFree (g,g->_class->size);
}


void dGeomGetAABB (dxGeom *g, dReal aabb[6])
{
  dAASSERT (g);
  g->_class->aabb (g,aabb);
}


dReal *dGeomGetSpaceAABB (dxGeom *g)
{
  dAASSERT (g);
  return g->space_aabb;
}

//****************************************************************************
// data for the standard classes

struct dxSphere {
  dReal radius;		// sphere radius
};

struct dxBox {
  dVector3 side;	// side lengths (x,y,z)
};

struct dxCCylinder {	// capped cylinder
  dReal radius,lz;	// radius, length along z axis */
};

struct dxPlane {
  dReal p[4];
};

struct dxGeomGroup {
  dArray<dxGeom*> parts;	// all the geoms that make up the group
};

//****************************************************************************
// primitive collision functions
// same interface as dCollide().
// S=sphere, B=box, C=capped cylinder, P=plane, G=group, T=transform

int dCollideSS (const dxGeom *o1, const dxGeom *o2, int flags,
		dContactGeom *contact, int skip)
{
  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dSphereClass);
  dIASSERT (o2->_class->num == dSphereClass);
  dxSphere *s1 = (dxSphere*) CLASSDATA(o1);
  dxSphere *s2 = (dxSphere*) CLASSDATA(o2);
  contact->g1 = const_cast<dxGeom*> (o1);
  contact->g2 = const_cast<dxGeom*> (o2);
  return dCollideSpheres (o1->pos,s1->radius,
			  o2->pos,s2->radius,contact);
}


int dCollideSB (const dxGeom *o1, const dxGeom *o2, int flags,
		dContactGeom *contact, int skip)
{
  // this is easy. get the sphere center `p' relative to the box, and then clip
  // that to the boundary of the box (call that point `q'). if q is on the
  // boundary of the box and |p-q| is <= sphere radius, they touch.
  // if q is inside the box, the sphere is inside the box, so set a contact
  // normal to push the sphere to the closest box edge.

  dVector3 l,t,p,q,r;
  dReal depth;
  int onborder = 0;

  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dSphereClass);
  dIASSERT (o2->_class->num == dBoxClass);
  dxSphere *sphere = (dxSphere*) CLASSDATA(o1);
  dxBox *box = (dxBox*) CLASSDATA(o2);

  contact->g1 = const_cast<dxGeom*> (o1);
  contact->g2 = const_cast<dxGeom*> (o2);

  p[0] = o1->pos[0] - o2->pos[0];
  p[1] = o1->pos[1] - o2->pos[1];
  p[2] = o1->pos[2] - o2->pos[2];

  l[0] = box->side[0]*REAL(0.5);
  t[0] = dDOT14(p,o2->R);
  if (t[0] < -l[0]) { t[0] = -l[0]; onborder = 1; }
  if (t[0] >  l[0]) { t[0] =  l[0]; onborder = 1; }

  l[1] = box->side[1]*REAL(0.5);
  t[1] = dDOT14(p,o2->R+1);
  if (t[1] < -l[1]) { t[1] = -l[1]; onborder = 1; }
  if (t[1] >  l[1]) { t[1] =  l[1]; onborder = 1; }

  t[2] = dDOT14(p,o2->R+2);
  l[2] = box->side[2]*REAL(0.5);
  if (t[2] < -l[2]) { t[2] = -l[2]; onborder = 1; }
  if (t[2] >  l[2]) { t[2] =  l[2]; onborder = 1; }

  if (!onborder) {
    // sphere center inside box. find largest `t' value
    dReal max = dFabs(t[0]);
    int maxi = 0;
    for (int i=1; i<3; i++) {
      dReal tt = dFabs(t[i]);
      if (tt > max) {
	max = tt;
	maxi = i;
      }
    }
    // contact position = sphere center
    contact->pos[0] = o1->pos[0];
    contact->pos[1] = o1->pos[1];
    contact->pos[2] = o1->pos[2];
    // contact normal aligned with box edge along largest `t' value
    dVector3 tmp;
    tmp[0] = 0;
    tmp[1] = 0;
    tmp[2] = 0;
    tmp[maxi] = (t[maxi] > 0) ? REAL(1.0) : REAL(-1.0);
    dMULTIPLY0_331 (contact->normal,o2->R,tmp);
    // contact depth = distance to wall along normal plus radius
    contact->depth = l[maxi] - max + sphere->radius;
    return 1;
  }

  t[3] = 0;			//@@@ hmmm
  dMULTIPLY0_331 (q,o2->R,t);
  r[0] = p[0] - q[0];
  r[1] = p[1] - q[1];
  r[2] = p[2] - q[2];
  depth = sphere->radius - dSqrt(dDOT(r,r));
  if (depth < 0) return 0;
  contact->pos[0] = q[0] + o2->pos[0];
  contact->pos[1] = q[1] + o2->pos[1];
  contact->pos[2] = q[2] + o2->pos[2];
  contact->normal[0] = r[0];
  contact->normal[1] = r[1];
  contact->normal[2] = r[2];
  dNormalize3 (contact->normal);
  contact->depth = depth;
  return 1;
}


int dCollideSP (const dxGeom *o1, const dxGeom *o2, int flags,
		dContactGeom *contact, int skip)
{
  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dSphereClass);
  dIASSERT (o2->_class->num == dPlaneClass);
  contact->g1 = const_cast<dxGeom*> (o1);
  contact->g2 = const_cast<dxGeom*> (o2);
  dxSphere *sphere = (dxSphere*) CLASSDATA(o1);
  dxPlane *plane = (dxPlane*) CLASSDATA(o2);
  dReal k = dDOT (o1->pos,plane->p);
  dReal depth = plane->p[3] - k + sphere->radius;
  if (depth >= 0) {
    contact->normal[0] = plane->p[0];
    contact->normal[1] = plane->p[1];
    contact->normal[2] = plane->p[2];
    contact->pos[0] = o1->pos[0] - plane->p[0] * sphere->radius;
    contact->pos[1] = o1->pos[1] - plane->p[1] * sphere->radius;
    contact->pos[2] = o1->pos[2] - plane->p[2] * sphere->radius;
    contact->depth = depth;
    return 1;
  }
  else return 0;
}


int dCollideBB (const dxGeom *o1, const dxGeom *o2, int flags,
		dContactGeom *contact, int skip)
{
  dVector3 normal;
  dReal depth;
  int code;
  dxBox *b1 = (dxBox*) CLASSDATA(o1);
  dxBox *b2 = (dxBox*) CLASSDATA(o2);
  int num = dBoxBox (o1->pos,o1->R,b1->side, o2->pos,o2->R,b2->side,
		     normal,&depth,&code,flags & NUMC_MASK,contact,skip);
  for (int i=0; i<num; i++) {
    CONTACT(contact,i*skip)->normal[0] = -normal[0];
    CONTACT(contact,i*skip)->normal[1] = -normal[1];
    CONTACT(contact,i*skip)->normal[2] = -normal[2];
    CONTACT(contact,i*skip)->g1 = const_cast<dxGeom*> (o1);
    CONTACT(contact,i*skip)->g2 = const_cast<dxGeom*> (o2);
  }
  return num;
}


int dCollideBP (const dxGeom *o1, const dxGeom *o2,
		int flags, dContactGeom *contact, int skip)
{
  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dBoxClass);
  dIASSERT (o2->_class->num == dPlaneClass);
  contact->g1 = const_cast<dxGeom*> (o1);
  contact->g2 = const_cast<dxGeom*> (o2);
  dxBox *box = (dxBox*) CLASSDATA(o1);
  dxPlane *plane = (dxPlane*) CLASSDATA(o2);
  int ret = 0;

  //@@@ problem: using 4-vector (plane->p) as 3-vector (normal).
  const dReal *R = o1->R;		// rotation of box
  const dReal *n = plane->p;		// normal vector

  // project sides lengths along normal vector, get absolute values
  dReal Q1 = dDOT14(n,R+0);
  dReal Q2 = dDOT14(n,R+1);
  dReal Q3 = dDOT14(n,R+2);
  dReal A1 = box->side[0] * Q1;
  dReal A2 = box->side[1] * Q2;
  dReal A3 = box->side[2] * Q3;
  dReal B1 = dFabs(A1);
  dReal B2 = dFabs(A2);
  dReal B3 = dFabs(A3);

  // early exit test
  dReal depth = plane->p[3] + REAL(0.5)*(B1+B2+B3) - dDOT(n,o1->pos);
  if (depth < 0) return 0;

  // find number of contacts requested
  int maxc = flags & NUMC_MASK;
  if (maxc < 1) maxc = 1;
  if (maxc > 3) maxc = 3;	// no more than 3 contacts per box allowed

  // find deepest point
  dVector3 p;
  p[0] = o1->pos[0];
  p[1] = o1->pos[1];
  p[2] = o1->pos[2];
#define FOO(i,op) \
  p[0] op REAL(0.5)*box->side[i] * R[0+i]; \
  p[1] op REAL(0.5)*box->side[i] * R[4+i]; \
  p[2] op REAL(0.5)*box->side[i] * R[8+i];
#define BAR(i,iinc) if (A ## iinc > 0) { FOO(i,-=) } else { FOO(i,+=) }
  BAR(0,1);
  BAR(1,2);
  BAR(2,3);
#undef FOO
#undef BAR

  // the deepest point is the first contact point
  contact->pos[0] = p[0];
  contact->pos[1] = p[1];
  contact->pos[2] = p[2];
  contact->normal[0] = n[0];
  contact->normal[1] = n[1];
  contact->normal[2] = n[2];
  contact->depth = depth;
  ret = 1;		// ret is number of contact points found so far
  if (maxc == 1) goto done;

  // get the second and third contact points by starting from `p' and going
  // along the two sides with the smallest projected length.

#define FOO(i,j,op) \
  CONTACT(contact,i*skip)->pos[0] = p[0] op box->side[j] * R[0+j]; \
  CONTACT(contact,i*skip)->pos[1] = p[1] op box->side[j] * R[4+j]; \
  CONTACT(contact,i*skip)->pos[2] = p[2] op box->side[j] * R[8+j];
#define BAR(ctact,side,sideinc) \
  depth -= B ## sideinc; \
  if (depth < 0) goto done; \
  if (A ## sideinc > 0) { FOO(ctact,side,+) } else { FOO(ctact,side,-) } \
  CONTACT(contact,ctact*skip)->depth = depth; \
  ret++;

  CONTACT(contact,skip)->normal[0] = n[0];
  CONTACT(contact,skip)->normal[1] = n[1];
  CONTACT(contact,skip)->normal[2] = n[2];
  if (maxc == 3) {
    CONTACT(contact,2*skip)->normal[0] = n[0];
    CONTACT(contact,2*skip)->normal[1] = n[1];
    CONTACT(contact,2*skip)->normal[2] = n[2];
  }

  if (B1 < B2) {
    if (B3 < B1) goto use_side_3; else {
      BAR(1,0,1);	// use side 1
      if (maxc == 2) goto done;
      if (B2 < B3) goto contact2_2; else goto contact2_3;
    }
  }
  else {
    if (B3 < B2) {
      use_side_3:	// use side 3
      BAR(1,2,3);
      if (maxc == 2) goto done;
      if (B1 < B2) goto contact2_1; else goto contact2_2;
    }
    else {
      BAR(1,1,2);	// use side 2
      if (maxc == 2) goto done;
      if (B1 < B3) goto contact2_1; else goto contact2_3;
    }
  }

  contact2_1: BAR(2,0,1); goto done;
  contact2_2: BAR(2,1,2); goto done;
  contact2_3: BAR(2,2,3); goto done;
#undef FOO
#undef BAR

 done:
  for (int i=0; i<ret; i++) {
    CONTACT(contact,i*skip)->g1 = const_cast<dxGeom*> (o1);
    CONTACT(contact,i*skip)->g2 = const_cast<dxGeom*> (o2);
  }
  return ret;
}


int dCollideCS (const dxGeom *o1, const dxGeom *o2, int flags,
		dContactGeom *contact, int skip)
{
  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dCCylinderClass);
  dIASSERT (o2->_class->num == dSphereClass);
  contact->g1 = const_cast<dxGeom*> (o1);
  contact->g2 = const_cast<dxGeom*> (o2);
  dxCCylinder *ccyl = (dxCCylinder*) CLASSDATA(o1);
  dxSphere *sphere = (dxSphere*) CLASSDATA(o2);

  // find the point on the cylinder axis that is closest to the sphere
  dReal alpha = 
    o1->R[2]  * (o2->pos[0] - o1->pos[0]) +
    o1->R[6]  * (o2->pos[1] - o1->pos[1]) +
    o1->R[10] * (o2->pos[2] - o1->pos[2]);
  dReal lz2 = ccyl->lz * REAL(0.5);
  if (alpha > lz2) alpha = lz2;
  if (alpha < -lz2) alpha = -lz2;

  // collide the spheres
  dVector3 p;
  p[0] = o1->pos[0] + alpha * o1->R[2];
  p[1] = o1->pos[1] + alpha * o1->R[6];
  p[2] = o1->pos[2] + alpha * o1->R[10];
  return dCollideSpheres (p,ccyl->radius,o2->pos,sphere->radius,contact);
}


int dCollideCB (const dxGeom *o1, const dxGeom *o2, int flags,
		dContactGeom *contact, int skip)
{
  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dCCylinderClass);
  dIASSERT (o2->_class->num == dBoxClass);
  contact->g1 = const_cast<dxGeom*> (o1);
  contact->g2 = const_cast<dxGeom*> (o2);
  dxCCylinder *cyl = (dxCCylinder*) CLASSDATA(o1);
  dxBox *box = (dxBox*) CLASSDATA(o2);

  // get p1,p2 = cylinder axis endpoints, get radius
  dVector3 p1,p2;
  dReal clen = cyl->lz * REAL(0.5);
  p1[0] = o1->pos[0] + clen * o1->R[2];
  p1[1] = o1->pos[1] + clen * o1->R[6];
  p1[2] = o1->pos[2] + clen * o1->R[10];
  p2[0] = o1->pos[0] - clen * o1->R[2];
  p2[1] = o1->pos[1] - clen * o1->R[6];
  p2[2] = o1->pos[2] - clen * o1->R[10];
  dReal radius = cyl->radius;

  // copy out box center, rotation matrix, and side array
  dReal *c = o2->pos;
  dReal *R = o2->R;
  dReal *side = box->side;

  // get the closest point between the cylinder axis and the box
  dVector3 pl,pb;
  dClosestLineBoxPoints (p1,p2,c,R,side,pl,pb);

  // generate contact point
  return dCollideSpheres (pl,radius,pb,0,contact);
}


// this returns at most one contact point when the two cylinder's axes are not
// aligned, and at most two (for stability) when they are aligned.
// the algorithm minimizes the distance between two "sample spheres" that are
// positioned along the cylinder axes according to:
//    sphere1 = pos1 + alpha1 * axis1
//    sphere2 = pos2 + alpha2 * axis2
// alpha1 and alpha2 are limited to +/- half the length of the cylinders.
// the algorithm works by finding a solution that has both alphas free, or
// a solution that has one or both alphas fixed to the ends of the cylinder.

int dCollideCC (const dxGeom *o1, const dxGeom *o2,
		int flags, dContactGeom *contact, int skip)
{
  int i;
  const dReal tolerance = REAL(1e-5);

  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dCCylinderClass);
  dIASSERT (o2->_class->num == dCCylinderClass);
  contact->g1 = const_cast<dxGeom*> (o1);
  contact->g2 = const_cast<dxGeom*> (o2);
  dxCCylinder *cyl1 = (dxCCylinder*) CLASSDATA(o1);
  dxCCylinder *cyl2 = (dxCCylinder*) CLASSDATA(o2);

  // copy out some variables, for convenience
  dReal lz1 = cyl1->lz * REAL(0.5);
  dReal lz2 = cyl2->lz * REAL(0.5);
  dReal *pos1 = o1->pos;
  dReal *pos2 = o2->pos;
  dReal axis1[3],axis2[3];
  axis1[0] = o1->R[2];
  axis1[1] = o1->R[6];
  axis1[2] = o1->R[10];
  axis2[0] = o2->R[2];
  axis2[1] = o2->R[6];
  axis2[2] = o2->R[10];

  dReal alpha1,alpha2,sphere1[3],sphere2[3];
  int fix1 = 0;		// 0 if alpha1 is free, +/-1 to fix at +/- lz1
  int fix2 = 0;		// 0 if alpha2 is free, +/-1 to fix at +/- lz2

  for (int count=0; count<9; count++) {
    // find a trial solution by fixing or not fixing the alphas
    if (fix1) {
      if (fix2) {
	// alpha1 and alpha2 are fixed, so the solution is easy
	if (fix1 > 0) alpha1 = lz1; else alpha1 = -lz1;
	if (fix2 > 0) alpha2 = lz2; else alpha2 = -lz2;
	for (i=0; i<3; i++) sphere1[i] = pos1[i] + alpha1*axis1[i];
	for (i=0; i<3; i++) sphere2[i] = pos2[i] + alpha2*axis2[i];
      }
      else {
	// fix alpha1 but let alpha2 be free
	if (fix1 > 0) alpha1 = lz1; else alpha1 = -lz1;
	for (i=0; i<3; i++) sphere1[i] = pos1[i] + alpha1*axis1[i];
	alpha2 = (axis2[0]*(sphere1[0]-pos2[0]) +
		  axis2[1]*(sphere1[1]-pos2[1]) +
		  axis2[2]*(sphere1[2]-pos2[2]));
	for (i=0; i<3; i++) sphere2[i] = pos2[i] + alpha2*axis2[i];
      }
    }
    else {
      if (fix2) {
	// fix alpha2 but let alpha1 be free
	if (fix2 > 0) alpha2 = lz2; else alpha2 = -lz2;
	for (i=0; i<3; i++) sphere2[i] = pos2[i] + alpha2*axis2[i];
	alpha1 = (axis1[0]*(sphere2[0]-pos1[0]) +
		  axis1[1]*(sphere2[1]-pos1[1]) +
		  axis1[2]*(sphere2[2]-pos1[2]));
	for (i=0; i<3; i++) sphere1[i] = pos1[i] + alpha1*axis1[i];
      }
      else {
	// let alpha1 and alpha2 be free
	// compute determinant of d(d^2)\d(alpha) jacobian
	dReal a1a2 = dDOT (axis1,axis2);
	dReal det = REAL(1.0)-a1a2*a1a2;
	if (det < tolerance) {
	  // the cylinder axes (almost) parallel, so we will generate up to two
	  // contacts. the solution matrix is rank deficient so alpha1 and
	  // alpha2 are related by:
	  //       alpha2 =   alpha1 + (pos1-pos2)'*axis1   (if axis1==axis2)
	  //    or alpha2 = -(alpha1 + (pos1-pos2)'*axis1)  (if axis1==-axis2)
	  // first compute where the two cylinders overlap in alpha1 space:
	  if (a1a2 < 0) {
	    axis2[0] = -axis2[0];
	    axis2[1] = -axis2[1];
	    axis2[2] = -axis2[2];
	  }
	  dReal q[3];
	  for (i=0; i<3; i++) q[i] = pos1[i]-pos2[i];
	  dReal k = dDOT (axis1,q);
	  dReal a1lo = -lz1;
	  dReal a1hi = lz1;
	  dReal a2lo = -lz2 - k;
	  dReal a2hi = lz2 - k;
	  dReal lo = (a1lo > a2lo) ? a1lo : a2lo;
	  dReal hi = (a1hi < a2hi) ? a1hi : a2hi;
	  if (lo <= hi) {
	    int num_contacts = flags & NUMC_MASK;
	    if (num_contacts >= 2 && lo < hi) {
	      // generate up to two contacts. if one of those contacts is
	      // not made, fall back on the one-contact strategy.
	      for (i=0; i<3; i++) sphere1[i] = pos1[i] + lo*axis1[i];
	      for (i=0; i<3; i++) sphere2[i] = pos2[i] + (lo+k)*axis2[i];
	      int n1 = dCollideSpheres (sphere1,cyl1->radius,
					sphere2,cyl2->radius,contact);
	      if (n1) {
		for (i=0; i<3; i++) sphere1[i] = pos1[i] + hi*axis1[i];
		for (i=0; i<3; i++) sphere2[i] = pos2[i] + (hi+k)*axis2[i];
		dContactGeom *c2 = CONTACT(contact,skip);
		int n2 = dCollideSpheres (sphere1,cyl1->radius,
					  sphere2,cyl2->radius, c2);
		if (n2) {
		  c2->g1 = const_cast<dxGeom*> (o1);
		  c2->g2 = const_cast<dxGeom*> (o2);
		  return 2;
		}
	      }
	    }

	    // just one contact to generate, so put it in the middle of
	    // the range
	    alpha1 = (lo + hi) * REAL(0.5);
	    alpha2 = alpha1 + k;
	    for (i=0; i<3; i++) sphere1[i] = pos1[i] + alpha1*axis1[i];
	    for (i=0; i<3; i++) sphere2[i] = pos2[i] + alpha2*axis2[i];
	    return dCollideSpheres (sphere1,cyl1->radius,
				    sphere2,cyl2->radius,contact);
	  }
	  else return 0;
	}
	det = REAL(1.0)/det;
	dReal delta[3];
	for (i=0; i<3; i++) delta[i] = pos1[i] - pos2[i];
	dReal q1 = dDOT (delta,axis1);
	dReal q2 = dDOT (delta,axis2);
	alpha1 = det*(a1a2*q2-q1);
	alpha2 = det*(q2-a1a2*q1);
	for (i=0; i<3; i++) sphere1[i] = pos1[i] + alpha1*axis1[i];
	for (i=0; i<3; i++) sphere2[i] = pos2[i] + alpha2*axis2[i];
      }
    }

    // if the alphas are outside their allowed ranges then fix them and
    // try again
    if (fix1==0) {
      if (alpha1 < -lz1) {
	fix1 = -1;
	continue;
      }
      if (alpha1 > lz1) {
	fix1 = 1;
	continue;
      }
    }
    if (fix2==0) {
      if (alpha2 < -lz2) {
	fix2 = -1;
	continue;
      }
      if (alpha2 > lz2) {
	fix2 = 1;
	continue;
      }
    }

    // unfix the alpha variables if the local distance gradient indicates
    // that we are not yet at the minimum
    dReal tmp[3];
    for (i=0; i<3; i++) tmp[i] = sphere1[i] - sphere2[i];
    if (fix1) {
      dReal gradient = dDOT (tmp,axis1);
      if ((fix1 > 0 && gradient > 0) || (fix1 < 0 && gradient < 0)) {
	fix1 = 0;
	continue;
      }
    }
    if (fix2) {
      dReal gradient = -dDOT (tmp,axis2);
      if ((fix2 > 0 && gradient > 0) || (fix2 < 0 && gradient < 0)) {
	fix2 = 0;
	continue;
      }
    }
    return dCollideSpheres (sphere1,cyl1->radius,sphere2,cyl2->radius,contact);
  }
  // if we go through the loop too much, then give up. we should NEVER get to
  // this point (i hope).
  dMessage (0,"dCollideCC(): too many iterations");
  return 0;
}


int dCollideCP (const dxGeom *o1, const dxGeom *o2, int flags,
		dContactGeom *contact, int skip)
{
  dIASSERT (skip >= (int)sizeof(dContactGeom));
  dIASSERT (o1->_class->num == dCCylinderClass);
  dIASSERT (o2->_class->num == dPlaneClass);
  dxCCylinder *ccyl = (dxCCylinder*) CLASSDATA(o1);
  dxPlane *plane = (dxPlane*) CLASSDATA(o2);

  // collide the deepest capping sphere with the plane
  dReal sign = (dDOT14 (plane->p,o1->R+2) > 0) ? REAL(-1.0) : REAL(1.0);
  dVector3 p;
  p[0] = o1->pos[0] + o1->R[2]  * ccyl->lz * REAL(0.5) * sign;
  p[1] = o1->pos[1] + o1->R[6]  * ccyl->lz * REAL(0.5) * sign;
  p[2] = o1->pos[2] + o1->R[10] * ccyl->lz * REAL(0.5) * sign;

  dReal k = dDOT (p,plane->p);
  dReal depth = plane->p[3] - k + ccyl->radius;
  if (depth < 0) return 0;
  contact->normal[0] = plane->p[0];
  contact->normal[1] = plane->p[1];
  contact->normal[2] = plane->p[2];
  contact->pos[0] = p[0] - plane->p[0] * ccyl->radius;
  contact->pos[1] = p[1] - plane->p[1] * ccyl->radius;
  contact->pos[2] = p[2] - plane->p[2] * ccyl->radius;
  contact->depth = depth;

  int ncontacts = 1;
  if ((flags & NUMC_MASK) >= 2) {
    // collide the other capping sphere with the plane
    p[0] = o1->pos[0] - o1->R[2]  * ccyl->lz * REAL(0.5) * sign;
    p[1] = o1->pos[1] - o1->R[6]  * ccyl->lz * REAL(0.5) * sign;
    p[2] = o1->pos[2] - o1->R[10] * ccyl->lz * REAL(0.5) * sign;

    k = dDOT (p,plane->p);
    depth = plane->p[3] - k + ccyl->radius;
    if (depth >= 0) {
      dContactGeom *c2 = CONTACT(contact,skip);
      c2->normal[0] = plane->p[0];
      c2->normal[1] = plane->p[1];
      c2->normal[2] = plane->p[2];
      c2->pos[0] = p[0] - plane->p[0] * ccyl->radius;
      c2->pos[1] = p[1] - plane->p[1] * ccyl->radius;
      c2->pos[2] = p[2] - plane->p[2] * ccyl->radius;
      c2->depth = depth;
      ncontacts = 2;
    }
  }

  for (int i=0; i < ncontacts; i++) {
    CONTACT(contact,i*skip)->g1 = const_cast<dxGeom*> (o1);
    CONTACT(contact,i*skip)->g2 = const_cast<dxGeom*> (o2);
  }
  return ncontacts;
}


// this collides a group with another geom. the other geom can also be a
// group, but this case is not handled specially.

int dCollideG (const dxGeom *o1, const dxGeom *o2, int flags,
	       dContactGeom *contact, int skip)
{
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(o1);
  int numleft = flags & NUMC_MASK;
  if (numleft == 0) numleft = 1;
  flags &= ~NUMC_MASK;
  int num=0,i=0;
  while (i < gr->parts.size() && numleft > 0) {
    int n = dCollide (gr->parts[i],const_cast<dxGeom*>(o2),
		      flags | numleft,contact,skip);
    contact = CONTACT (contact,skip*n);
    numleft -= n;
    num += n;
    i++;
  }
  return num;
}

//****************************************************************************
// standard classes

SHAREDLIBEXPORT int dSphereClass = -1;
SHAREDLIBEXPORT int dBoxClass = -1;
SHAREDLIBEXPORT int dCCylinderClass = -1;
SHAREDLIBEXPORT int dPlaneClass = -1;


static dColliderFn * dSphereColliderFn (int num)
{
  if (num == dSphereClass) return (dColliderFn *) &dCollideSS;
  if (num == dBoxClass) return (dColliderFn *) &dCollideSB;
  if (num == dPlaneClass) return (dColliderFn *) &dCollideSP;
  return 0;
}


static void dSphereAABB (dxGeom *geom, dReal aabb[6])
{
  dxSphere *s = (dxSphere*) CLASSDATA(geom);
  aabb[0] = geom->pos[0] - s->radius;
  aabb[1] = geom->pos[0] + s->radius;
  aabb[2] = geom->pos[1] - s->radius;
  aabb[3] = geom->pos[1] + s->radius;
  aabb[4] = geom->pos[2] - s->radius;
  aabb[5] = geom->pos[2] + s->radius;
}


static dColliderFn * dBoxColliderFn (int num)
{
  if (num == dBoxClass) return (dColliderFn *) &dCollideBB;
  if (num == dPlaneClass) return (dColliderFn *) &dCollideBP;
  return 0;
}


static void dBoxAABB (dxGeom *geom, dReal aabb[6])
{
  dxBox *b = (dxBox*) CLASSDATA(geom);
  dReal xrange = REAL(0.5) * (dFabs (geom->R[0] * b->side[0]) +
    dFabs (geom->R[1] * b->side[1]) + dFabs (geom->R[2] * b->side[2]));
  dReal yrange = REAL(0.5) * (dFabs (geom->R[4] * b->side[0]) +
    dFabs (geom->R[5] * b->side[1]) + dFabs (geom->R[6] * b->side[2]));
  dReal zrange = REAL(0.5) * (dFabs (geom->R[8] * b->side[0]) +
    dFabs (geom->R[9] * b->side[1]) + dFabs (geom->R[10] * b->side[2]));
  aabb[0] = geom->pos[0] - xrange;
  aabb[1] = geom->pos[0] + xrange;
  aabb[2] = geom->pos[1] - yrange;
  aabb[3] = geom->pos[1] + yrange;
  aabb[4] = geom->pos[2] - zrange;
  aabb[5] = geom->pos[2] + zrange;
}


static dColliderFn * dCCylinderColliderFn (int num)
{
  if (num == dSphereClass) return (dColliderFn *) &dCollideCS;
  if (num == dPlaneClass) return (dColliderFn *) &dCollideCP;
  if (num == dCCylinderClass) return (dColliderFn *) &dCollideCC;
  if (num == dBoxClass) return (dColliderFn *) &dCollideCB;
  return 0;
}


static void dCCylinderAABB (dxGeom *geom, dReal aabb[6])
{
  dxCCylinder *c = (dxCCylinder*) CLASSDATA(geom);
  dReal xrange = dFabs(geom->R[2]  * c->lz) * REAL(0.5) + c->radius;
  dReal yrange = dFabs(geom->R[6]  * c->lz) * REAL(0.5) + c->radius;
  dReal zrange = dFabs(geom->R[10] * c->lz) * REAL(0.5) + c->radius;
  aabb[0] = geom->pos[0] - xrange;
  aabb[1] = geom->pos[0] + xrange;
  aabb[2] = geom->pos[1] - yrange;
  aabb[3] = geom->pos[1] + yrange;
  aabb[4] = geom->pos[2] - zrange;
  aabb[5] = geom->pos[2] + zrange;
}


dColliderFn * dPlaneColliderFn (int num)
{
  return 0;
}


static void dPlaneAABB (dxGeom *geom, dReal aabb[6])
{
  // @@@ planes that have normal vectors aligned along an axis can use a
  // @@@ less comprehensive bounding box.
  aabb[0] = -dInfinity;
  aabb[1] = dInfinity;
  aabb[2] = -dInfinity;
  aabb[3] = dInfinity;
  aabb[4] = -dInfinity;
  aabb[5] = dInfinity;
}


dxGeom *dCreateSphere (dSpaceID space, dReal radius)
{
  dAASSERT (radius > 0);
  if (dSphereClass == -1) {
    dGeomClass c;
    c.bytes = sizeof (dxSphere);
    c.collider = &dSphereColliderFn;
    c.aabb = &dSphereAABB;
    c.aabb_test = 0;
    c.dtor = 0;
    dSphereClass = dCreateGeomClass (&c);
  }

  dxGeom *g = dCreateGeom (dSphereClass);
  if (space) dSpaceAdd (space,g);
  dxSphere *s = (dxSphere*) CLASSDATA(g);
  s->radius = radius;
  return g;
}


dxGeom *dCreateBox (dSpaceID space, dReal lx, dReal ly, dReal lz)
{
  dAASSERT (lx > 0 && ly > 0 && lz > 0);
  if (dBoxClass == -1) {
    dGeomClass c;
    c.bytes = sizeof (dxBox);
    c.collider = &dBoxColliderFn;
    c.aabb = &dBoxAABB;
    c.aabb_test = 0;
    c.dtor = 0;
    dBoxClass = dCreateGeomClass (&c);
  }

  dxGeom *g = dCreateGeom (dBoxClass);
  if (space) dSpaceAdd (space,g);
  dxBox *b = (dxBox*) CLASSDATA(g);
  b->side[0] = lx;
  b->side[1] = ly;
  b->side[2] = lz;
  return g;
}


dxGeom * dCreateCCylinder (dSpaceID space, dReal radius, dReal length)
{
  dAASSERT (radius > 0 && length > 0);
  if (dCCylinderClass == -1) {
    dGeomClass c;
    c.bytes = sizeof (dxCCylinder);
    c.collider = &dCCylinderColliderFn;
    c.aabb = &dCCylinderAABB;
    c.aabb_test = 0;
    c.dtor = 0;
    dCCylinderClass = dCreateGeomClass (&c);
  }

  dxGeom *g = dCreateGeom (dCCylinderClass);
  if (space) dSpaceAdd (space,g);
  dxCCylinder *c = (dxCCylinder*) CLASSDATA(g);
  c->radius = radius;
  c->lz = length;
  return g;
}


dxGeom *dCreatePlane (dSpaceID space,
		      dReal a, dReal b, dReal c, dReal d)
{
  if (dPlaneClass == -1) {
    dGeomClass c;
    c.bytes = sizeof (dxPlane);
    c.collider = &dPlaneColliderFn;
    c.aabb = &dPlaneAABB;
    c.aabb_test = 0;
    c.dtor = 0;
    dPlaneClass = dCreateGeomClass (&c);
  }

  dxGeom *g = dCreateGeom (dPlaneClass);
  if (space) dSpaceAdd (space,g);
  dxPlane *p = (dxPlane*) CLASSDATA(g);

  // make sure plane normal has unit length
  dReal l = a*a + b*b + c*c;
  if (l > 0) {
    l = dRecipSqrt(l);
    p->p[0] = a*l;
    p->p[1] = b*l;
    p->p[2] = c*l;
    p->p[3] = d*l;
  }
  else {
    p->p[0] = 1;
    p->p[1] = 0;
    p->p[2] = 0;
    p->p[3] = 0;
  }
  return g;
}


void dGeomSphereSetRadius (dGeomID g, dReal radius)
{
  dUASSERT (g && g->_class->num == dSphereClass,"argument not a sphere");
  dAASSERT (radius > 0);
  dxSphere *s = (dxSphere*) CLASSDATA(g);
  s->radius = radius;
}


void dGeomBoxSetLengths (dGeomID g, dReal lx, dReal ly, dReal lz)
{
  dUASSERT (g && g->_class->num == dBoxClass,"argument not a box");
  dAASSERT (lx > 0 && ly > 0 && lz > 0);
  dxBox *b = (dxBox*) CLASSDATA(g);
  b->side[0] = lx;
  b->side[1] = ly;
  b->side[2] = lz;
}


void dGeomPlaneSetParams (dGeomID g, dReal a, dReal b, dReal c, dReal d)
{
  dUASSERT (g && g->_class->num == dPlaneClass,"argument not a plane");
  dxPlane *p = (dxPlane*) CLASSDATA(g);
  p->p[0] = a;
  p->p[1] = b;
  p->p[2] = c;
  p->p[3] = d;
}


void dGeomCCylinderSetParams (dGeomID g, dReal radius, dReal length)
{
  dUASSERT (g && g->_class->num == dCCylinderClass,"argument not a ccylinder");
  dAASSERT (radius > 0 && length > 0);
  dxCCylinder *c = (dxCCylinder*) CLASSDATA(g);
  c->radius = radius;
  c->lz = length;
}


dReal dGeomSphereGetRadius (dGeomID g)
{
  dUASSERT (g && g->_class->num == dSphereClass,"argument not a sphere");
  dxSphere *s = (dxSphere*) CLASSDATA(g);
  return s->radius;
}


void  dGeomBoxGetLengths (dGeomID g, dVector3 result)
{
  dUASSERT (g && g->_class->num == dBoxClass,"argument not a box");
  dxBox *b = (dxBox*) CLASSDATA(g);
  result[0] = b->side[0];
  result[1] = b->side[1];
  result[2] = b->side[2];
}


void  dGeomPlaneGetParams (dGeomID g, dVector4 result)
{
  dUASSERT (g && g->_class->num == dPlaneClass,"argument not a plane");
  dxPlane *p = (dxPlane*) CLASSDATA(g);
  result[0] = p->p[0];
  result[1] = p->p[1];
  result[2] = p->p[2];
  result[3] = p->p[3];
}


void dGeomCCylinderGetParams (dGeomID g, dReal *radius, dReal *length)
{
  dUASSERT (g && g->_class->num == dCCylinderClass,"argument not a ccylinder");
  dxCCylinder *c = (dxCCylinder*) CLASSDATA(g);
  *radius = c->radius;
  *length = c->lz;
}

//****************************************************************************
// geom group

int dGeomGroupClass = -1;

static dColliderFn * dGeomGroupColliderFn (int num)
{
  return (dColliderFn *) &dCollideG;
}


static void dGeomGroupAABB (dxGeom *geom, dReal aabb[6])
{
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(geom);
  aabb[0] = dInfinity;
  aabb[1] = -dInfinity;
  aabb[2] = dInfinity;
  aabb[3] = -dInfinity;
  aabb[4] = dInfinity;
  aabb[5] = -dInfinity;
  int i,j;
  for (i=0; i < gr->parts.size(); i++) {
    dReal aabb2[6];
    gr->parts[i]->_class->aabb (gr->parts[i],aabb2);
    for (j=0; j<6; j += 2) if (aabb2[j] < aabb[j]) aabb[j] = aabb2[j];
    for (j=1; j<6; j += 2) if (aabb2[j] > aabb[j]) aabb[j] = aabb2[j];
  }
}


static void dGeomGroupDtor (dxGeom *geom)
{
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(geom);
  gr->parts.~dArray();
}


dxGeom *dCreateGeomGroup (dSpaceID space)
{
  if (dGeomGroupClass == -1) {
    dGeomClass c;
    c.bytes = sizeof (dxGeomGroup);
    c.collider = &dGeomGroupColliderFn;
    c.aabb = &dGeomGroupAABB;
    c.aabb_test = 0;
    c.dtor = &dGeomGroupDtor;
    dGeomGroupClass = dCreateGeomClass (&c);
  }

  dxGeom *g = dCreateGeom (dGeomGroupClass);
  if (space) dSpaceAdd (space,g);
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(g);
  gr->parts.constructor();
  return g;
}


void dGeomGroupAdd (dxGeom *g, dxGeom *x)
{
  dUASSERT (g && g->_class->num == dGeomGroupClass,"argument not a geomgroup");
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(g);
  gr->parts.push (x);
}


void dGeomGroupRemove (dxGeom *g, dxGeom *x)
{
  dUASSERT (g && g->_class->num == dGeomGroupClass,"argument not a geomgroup");
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(g);
  for (int i=0; i < gr->parts.size(); i++) {
    if (gr->parts[i] == x) {
      gr->parts.remove (i);
      return;
    }
  }
}


int dGeomGroupGetNumGeoms (dxGeom *g)
{
  dUASSERT (g && g->_class->num == dGeomGroupClass,"argument not a geomgroup");
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(g);
  return gr->parts.size();
}


dxGeom * dGeomGroupGetGeom (dxGeom *g, int i)
{
  dUASSERT (g && g->_class->num == dGeomGroupClass,"argument not a geomgroup");
  dxGeomGroup *gr = (dxGeomGroup*) CLASSDATA(g);
  dAASSERT (i >= 0 && i < gr->parts.size());
  return gr->parts[i];
}

//****************************************************************************
// transformed geom

int dGeomTransformClass = -1;

struct dxGeomTransform {
  dxGeom *obj;		// object that is being transformed
  int cleanup;		// 1 to destroy obj when destroyed
  int infomode;		// 1 to put Tx geom in dContactGeom g1
  dVector3 final_pos;	// final tx (body tx + relative tx) of the object.
  dMatrix3 final_R;	//   this is only set if the AABB function is called
};			//   by space collision before the collide fn is called


// compute final pos and R for the encapsulated geom object

static void compute_final_tx (const dxGeom *g)
{
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  dMULTIPLY0_331 (tr->final_pos,g->R,tr->obj->pos);
  tr->final_pos[0] += g->pos[0];
  tr->final_pos[1] += g->pos[1];
  tr->final_pos[2] += g->pos[2];
  dMULTIPLY0_333 (tr->final_R,g->R,tr->obj->R);
}



// this collides a transformed geom with another geom. the other geom can
// also be a transformed geom, but this case is not handled specially.

int dCollideT (const dxGeom *o1, const dxGeom *o2, int flags,
	       dContactGeom *contact, int skip)
{
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(o1);
  if (!tr->obj) return 0;
  dUASSERT (tr->obj->spaceid==0,
	    "GeomTransform encapsulated object must not be in a space");
  dUASSERT (tr->obj->body==0,
	    "GeomTransform encapsulated object must not be attach to a body");

  // backup the relative pos and R pointers of the encapsulated geom object,
  // and the body pointer
  dReal *posbak = tr->obj->pos;
  dReal *Rbak = tr->obj->R;
  dxBody *bodybak = tr->obj->body;

  // compute temporary pos and R for the encapsulated geom object
  if (!o1->space_aabb) compute_final_tx (o1);
  tr->obj->pos = tr->final_pos;
  tr->obj->R = tr->final_R;
  tr->obj->body = o1->body;

  // do the collision
  int n = dCollide (tr->obj,const_cast<dxGeom*>(o2),flags,contact,skip);

  // if required, adjust the 'g1' values in the generated contacts so that
  // thay indicated the GeomTransform object instead of the encapsulated
  // object.
  if (tr->infomode) {
    for (int i=0; i<n; i++) {
      dContactGeom *c = CONTACT(contact,skip*i);
      c->g1 = const_cast<dxGeom*> (o1);
    }
  }

  // restore the pos, R and body
  tr->obj->pos = posbak;
  tr->obj->R = Rbak;
  tr->obj->body = bodybak;
  return n;
}


static dColliderFn * dGeomTransformColliderFn (int num)
{
  return (dColliderFn *) &dCollideT;
}


static void dGeomTransformAABB (dxGeom *geom, dReal aabb[6])
{
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(geom);
  if (!tr->obj) {
    dSetZero (aabb,6);
    return;
  }

  // backup the relative pos and R pointers of the encapsulated geom object
  dReal *posbak = tr->obj->pos;
  dReal *Rbak = tr->obj->R;

  // compute temporary pos and R for the encapsulated geom object
  compute_final_tx (geom);
  tr->obj->pos = tr->final_pos;
  tr->obj->R = tr->final_R;

  // compute the AABB
  tr->obj->_class->aabb (tr->obj,aabb);

  // restore the pos and R
  tr->obj->pos = posbak;
  tr->obj->R = Rbak;
}


static void dGeomTransformDtor (dxGeom *geom)
{
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(geom);
  if (tr->obj && tr->cleanup) {
    dGeomDestroy (tr->obj);
  }
}


dxGeom *dCreateGeomTransform (dSpaceID space)
{
  if (dGeomTransformClass == -1) {
    dGeomClass c;
    c.bytes = sizeof (dxGeomTransform);
    c.collider = &dGeomTransformColliderFn;
    c.aabb = &dGeomTransformAABB;
    c.aabb_test = 0;
    c.dtor = dGeomTransformDtor;
    dGeomTransformClass = dCreateGeomClass (&c);
  }

  dxGeom *g = dCreateGeom (dGeomTransformClass);
  if (space) dSpaceAdd (space,g);

  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  tr->obj = 0;
  tr->cleanup = 0;
  tr->infomode = 0;
  dSetZero (tr->final_pos,4);
  dRSetIdentity (tr->final_R);

  return g;
}


void dGeomTransformSetGeom (dxGeom *g, dxGeom *obj)
{
  dUASSERT (g && g->_class->num == dGeomTransformClass,
	    "argument not a geom transform");
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  if (tr->obj && tr->cleanup) {
    dGeomDestroy (tr->obj);
  }
  tr->obj = obj;
}


dxGeom * dGeomTransformGetGeom (dxGeom *g)
{
  dUASSERT (g && g->_class->num == dGeomTransformClass,
	    "argument not a geom transform");
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  return tr->obj;
}


void dGeomTransformSetCleanup (dGeomID g, int mode)
{
  dUASSERT (g && g->_class->num == dGeomTransformClass,
	    "argument not a geom transform");
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  tr->cleanup = mode;
}


int dGeomTransformGetCleanup (dGeomID g)
{
  dUASSERT (g && g->_class->num == dGeomTransformClass,
	    "argument not a geom transform");
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  return tr->cleanup;
}


void dGeomTransformSetInfo (dGeomID g, int mode)
{
  dUASSERT (g && g->_class->num == dGeomTransformClass,
	    "argument not a geom transform");
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  tr->infomode = mode;
}


int dGeomTransformGetInfo (dGeomID g)
{
  dUASSERT (g && g->_class->num == dGeomTransformClass,
	    "argument not a geom transform");
  dxGeomTransform *tr = (dxGeomTransform*) CLASSDATA(g);
  return tr->infomode;
}

//****************************************************************************
// other utility functions

void dInfiniteAABB (dxGeom *geom, dReal aabb[6])
{
  aabb[0] = -dInfinity;
  aabb[1] = dInfinity;
  aabb[2] = -dInfinity;
  aabb[3] = dInfinity;
  aabb[4] = -dInfinity;
  aabb[5] = dInfinity;
}


void dCloseODE()
{
  if (colliders) {
    delete colliders;
    colliders = 0;
  }
  if (classes) {
    for (int i=0; i < classes->size(); i++) {
      dFree ((*classes)[i], sizeof (dxGeomClass));
    }
    delete classes;
    classes = 0;
  }

  // reset geom class vars
  dSphereClass = -1;
  dBoxClass = -1;
  dCCylinderClass = -1;
  dPlaneClass = -1;
  dGeomGroupClass = -1;
  dGeomTransformClass = -1;

  // if you're using contrib code you may want to uncomment the following:
  // dTriListClass = -1;
  // dRayClass = -1;
}
