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

design note: the general principle for giving a joint the option of connecting
to the static environment (i.e. the absolute frame) is to check the second
body (joint->node[1].body), and if it is zero then behave as if its body
transform is the identity.

*/

#include <ode/odemath.h>
#include <ode/rotation.h>
#include <ode/matrix.h>
#include "joint.h"

//****************************************************************************
// externs

extern "C" void dBodyAddTorque (dBodyID, dReal fx, dReal fy, dReal fz);
extern "C" void dBodyAddForce (dBodyID, dReal fx, dReal fy, dReal fz);

//****************************************************************************
// utility

// set three "ball-and-socket" rows in the constraint equation, and the
// corresponding right hand side.

static inline void setBall (dxJoint *joint, dxJoint::Info2 *info,
			    dVector3 anchor1, dVector3 anchor2)
{
  // anchor points in global coordinates with respect to body PORs.
  dVector3 a1,a2;

  int s = info->rowskip;

  // set jacobian
  info->J1l[0] = 1;
  info->J1l[s+1] = 1;
  info->J1l[2*s+2] = 1;
  dMULTIPLY0_331 (a1,joint->node[0].body->R,anchor1);
  dCROSSMAT (info->J1a,a1,s,-,+);
  if (joint->node[1].body) {
    info->J2l[0] = -1;
    info->J2l[s+1] = -1;
    info->J2l[2*s+2] = -1;
    dMULTIPLY0_331 (a2,joint->node[1].body->R,anchor2);
    dCROSSMAT (info->J2a,a2,s,+,-);
  }

  // set right hand side
  dReal k = info->fps * info->erp;
  if (joint->node[1].body) {
    for (int j=0; j<3; j++) {
      info->c[j] = k * (a2[j] + joint->node[1].body->pos[j] -
			a1[j] - joint->node[0].body->pos[j]);
    }
  }
  else {
    for (int j=0; j<3; j++) {
      info->c[j] = k * (anchor2[j] - a1[j] -
			joint->node[0].body->pos[j]);
    }
  }
}


// this is like setBall(), except that `axis' is a unit length vector
// (in global coordinates) that should be used for the first jacobian
// position row (the other two row vectors will be derived from this).
// `erp1' is the erp value to use along the axis.

static inline void setBall2 (dxJoint *joint, dxJoint::Info2 *info,
			    dVector3 anchor1, dVector3 anchor2,
			    dVector3 axis, dReal erp1)
{
  // anchor points in global coordinates with respect to body PORs.
  dVector3 a1,a2;

  int i,s = info->rowskip;

  // get vectors normal to the axis. in setBall() axis,q1,q2 is [1 0 0],
  // [0 1 0] and [0 0 1], which makes everything much easier.
  dVector3 q1,q2;
  dPlaneSpace (axis,q1,q2);

  // set jacobian
  for (i=0; i<3; i++) info->J1l[i] = axis[i];
  for (i=0; i<3; i++) info->J1l[s+i] = q1[i];
  for (i=0; i<3; i++) info->J1l[2*s+i] = q2[i];
  dMULTIPLY0_331 (a1,joint->node[0].body->R,anchor1);
  dCROSS (info->J1a,=,a1,axis);
  dCROSS (info->J1a+s,=,a1,q1);
  dCROSS (info->J1a+2*s,=,a1,q2);
  if (joint->node[1].body) {
    for (i=0; i<3; i++) info->J2l[i] = -axis[i];
    for (i=0; i<3; i++) info->J2l[s+i] = -q1[i];
    for (i=0; i<3; i++) info->J2l[2*s+i] = -q2[i];
    dMULTIPLY0_331 (a2,joint->node[1].body->R,anchor2);
    dCROSS (info->J2a,= -,a2,axis);
    dCROSS (info->J2a+s,= -,a2,q1);
    dCROSS (info->J2a+2*s,= -,a2,q2);
  }

  // set right hand side - measure error along (axis,q1,q2)
  dReal k1 = info->fps * erp1;
  dReal k = info->fps * info->erp;

  for (i=0; i<3; i++) a1[i] += joint->node[0].body->pos[i];
  if (joint->node[1].body) {
    for (i=0; i<3; i++) a2[i] += joint->node[1].body->pos[i];
    info->c[0] = k1 * (dDOT(axis,a2) - dDOT(axis,a1));
    info->c[1] = k * (dDOT(q1,a2) - dDOT(q1,a1));
    info->c[2] = k * (dDOT(q2,a2) - dDOT(q2,a1));
  }
  else {
    info->c[0] = k1 * (dDOT(axis,anchor2) - dDOT(axis,a1));
    info->c[1] = k * (dDOT(q1,anchor2) - dDOT(q1,a1));
    info->c[2] = k * (dDOT(q2,anchor2) - dDOT(q2,a1));
  }
}


// compute anchor points relative to bodies

static void setAnchors (dxJoint *j, dReal x, dReal y, dReal z,
			dVector3 anchor1, dVector3 anchor2)
{
  if (j->node[0].body) {
    dReal q[4];
    q[0] = x - j->node[0].body->pos[0];
    q[1] = y - j->node[0].body->pos[1];
    q[2] = z - j->node[0].body->pos[2];
    q[3] = 0;
    dMULTIPLY1_331 (anchor1,j->node[0].body->R,q);
    if (j->node[1].body) {
      q[0] = x - j->node[1].body->pos[0];
      q[1] = y - j->node[1].body->pos[1];
      q[2] = z - j->node[1].body->pos[2];
      q[3] = 0;
      dMULTIPLY1_331 (anchor2,j->node[1].body->R,q);
    }
    else {
      anchor2[0] = x;
      anchor2[1] = y;
      anchor2[2] = z;
    }
  }
  anchor1[3] = 0;
  anchor2[3] = 0;
}


// compute axes relative to bodies. axis2 can be 0

static void setAxes (dxJoint *j, dReal x, dReal y, dReal z,
		     dVector3 axis1, dVector3 axis2)
{
  if (j->node[0].body) {
    dReal q[4];
    q[0] = x;
    q[1] = y;
    q[2] = z;
    q[3] = 0;
    dNormalize3 (q);
    dMULTIPLY1_331 (axis1,j->node[0].body->R,q);
    if (axis2) {
      if (j->node[1].body) {
	dMULTIPLY1_331 (axis2,j->node[1].body->R,q);
      }
      else {
	axis2[0] = x;
	axis2[1] = y;
	axis2[2] = z;
      }
      axis2[3] = 0;
    }
  }
  axis1[3] = 0;
}


static void getAnchor (dxJoint *j, dVector3 result, dVector3 anchor1)
{
  if (j->node[0].body) {
    dMULTIPLY0_331 (result,j->node[0].body->R,anchor1);
    result[0] += j->node[0].body->pos[0];
    result[1] += j->node[0].body->pos[1];
    result[2] += j->node[0].body->pos[2];
  }
}


static void getAxis (dxJoint *j, dVector3 result, dVector3 axis1)
{
  if (j->node[0].body) {
    dMULTIPLY0_331 (result,j->node[0].body->R,axis1);
  }
}


// given two bodies (body1,body2), the hinge axis that they are connected by
// w.r.t. body1 (axis), and the initial relative orientation between them
// (q_initial), return the relative rotation angle. the initial relative
// orientation corresponds to an angle of zero. if body2 is 0 then measure the
// angle between body1 and the static frame.
//
// this will not return the correct angle if the bodies rotate along any axis
// other than the given hinge axis.

static dReal getHingeAngle (dxBody *body1, dxBody *body2, dVector3 axis,
			    dQuaternion q_initial)
{
  // the angle between the two bodies is extracted from the quaternion that
  // represents the relative rotation between them. recall that a quaternion
  // q is:
  //    [s,v] = [ cos(theta/2) , sin(theta/2) * u ]
  // where s is a scalar and v is a 3-vector. u is a unit length axis and
  // theta is a rotation along that axis. we can get theta/2 by:
  //    theta/2 = atan2 ( sin(theta/2) , cos(theta/2) )
  // but we can't get sin(theta/2) directly, only its absolute value, i.e.:
  //    |v| = |sin(theta/2)| * |u|
  //        = |sin(theta/2)|
  // using this value will have a strange effect. recall that there are two
  // quaternion representations of a given rotation, q and -q. typically as
  // a body rotates along the axis it will go through a complete cycle using
  // one representation and then the next cycle will use the other
  // representation. this corresponds to u pointing in the direction of the
  // hinge axis and then in the opposite direction. the result is that theta
  // will appear to go "backwards" every other cycle. here is a fix: if u
  // points "away" from the direction of the hinge (motor) axis (i.e. more
  // than 90 degrees) then use -q instead of q. this represents the same
  // rotation, but results in the cos(theta/2) value being sign inverted.

  // get qrel = relative rotation between the two bodies
  dQuaternion qrel;
  if (body2) {
    dQuaternion qq;
    dQMultiply1 (qq,body1->q,body2->q);
    dQMultiply2 (qrel,qq,q_initial);
  }
  else {
    // pretend body2->q is the identity
    dQMultiply3 (qrel,body1->q,q_initial);
  }

  // extract the angle from the quaternion. cost2 = cos(theta/2),
  // sint2 = |sin(theta/2)|
  dReal cost2 = qrel[0];
  dReal sint2 = dSqrt (qrel[1]*qrel[1]+qrel[2]*qrel[2]+qrel[3]*qrel[3]);
  dReal theta = (dDOT(qrel+1,axis) >= 0) ?	// @@@ padding assumptions
    (2 * dAtan2(sint2,cost2)) :		// if u points in direction of axis
    (2 * dAtan2(sint2,-cost2));		// if u points in opposite direction

  // the angle we get will be between 0..2*pi, but we want to return angles
  // between -pi..pi
  if (theta > M_PI) theta -= 2*M_PI;

  // the angle we've just extracted has the wrong sign
  theta = -theta;

  return theta;
}

//****************************************************************************
// dxJointLimitMotor

void dxJointLimitMotor::init (dxWorld *world)
{
  vel = 0;
  fmax = 0;
  lostop = -dInfinity;
  histop = dInfinity;
  fudge_factor = 1;
  normal_cfm = world->global_cfm;
  stop_erp = world->global_erp;
  stop_cfm = world->global_cfm;
  bounce = 0;
  limit = 0;
  limit_err = 0;
}


void dxJointLimitMotor::set (int num, dReal value)
{
  switch (num) {
  case dParamLoStop:
    if (value <= histop) lostop = value;
    break;
  case dParamHiStop:
    if (value >= lostop) histop = value;
    break;
  case dParamVel:
    vel = value;
    break;
  case dParamFMax:
    if (value >= 0) fmax = value;
    break;
  case dParamFudgeFactor:
    if (value >= 0 && value <= 1) fudge_factor = value;
    break;
  case dParamBounce:
    bounce = value;
    break;
  case dParamCFM:
    normal_cfm = value;
    break;
  case dParamStopERP:
    stop_erp = value;
    break;
  case dParamStopCFM:
    stop_cfm = value;
    break;
  }
}


dReal dxJointLimitMotor::get (int num)
{
  switch (num) {
  case dParamLoStop: return lostop;
  case dParamHiStop: return histop;
  case dParamVel: return vel;
  case dParamFMax: return fmax;
  case dParamFudgeFactor: return fudge_factor;
  case dParamBounce: return bounce;
  case dParamCFM: return normal_cfm;
  case dParamStopERP: return stop_erp;
  case dParamStopCFM: return stop_cfm;
  default: return 0;
  }
}


int dxJointLimitMotor::testRotationalLimit (dReal angle)
{
  if (angle <= lostop) {
    limit = 1;
    limit_err = angle - lostop;
    return 1;
  }
  else if (angle >= histop) {
    limit = 2;
    limit_err = angle - histop;
    return 1;
  }
  else {
    limit = 0;
    return 0;
  }
}


int dxJointLimitMotor::addLimot (dxJoint *joint,
				 dxJoint::Info2 *info, int row,
				 dVector3 ax1, int rotational)
{
  int srow = row * info->rowskip;

  // if the joint is powered, or has joint limits, add in the extra row
  int powered = fmax > 0;
  if (powered || limit) {
    dReal *J1 = rotational ? info->J1a : info->J1l;
    dReal *J2 = rotational ? info->J2a : info->J2l;

    J1[srow+0] = ax1[0];
    J1[srow+1] = ax1[1];
    J1[srow+2] = ax1[2];
    if (joint->node[1].body) {
      J2[srow+0] = -ax1[0];
      J2[srow+1] = -ax1[1];
      J2[srow+2] = -ax1[2];
    }

    // if we're limited low and high simultaneously, the joint motor is
    // ineffective
    if (limit && (lostop == histop)) powered = 0;

    if (powered) {
      info->cfm[row] = normal_cfm;
      if (! limit) {
	info->c[row] = vel;
	info->lo[row] = -fmax;
	info->hi[row] = fmax;
      }
      else {
	// the joint is at a limit, AND is being powered. if the joint is
	// being powered into the limit then we apply the maximum motor force
	// in that direction, because the motor is working against the
	// immovable limit. if the joint is being powered away from the limit
	// then we have problems because actually we need *two* lcp
	// constraints to handle this case. so we fake it and apply some
	// fraction of the maximum force. the fraction to use can be set as
	// a fudge factor.

	dReal fm = fmax;
	if (vel > 0) fm = -fm;

	// if we're powering away from the limit, apply the fudge factor
	if ((limit==1 && vel > 0) || (limit==2 && vel < 0)) fm *= fudge_factor;

	if (rotational) {
	  dBodyAddTorque (joint->node[0].body,-fm*ax1[0],-fm*ax1[1],
			  -fm*ax1[2]);
	  if (joint->node[1].body)
	    dBodyAddTorque (joint->node[1].body,fm*ax1[0],fm*ax1[1],fm*ax1[2]);
	}
	else {
	  dBodyAddForce (joint->node[0].body,-fm*ax1[0],-fm*ax1[1],-fm*ax1[2]);
	  if (joint->node[1].body)
	    dBodyAddForce (joint->node[1].body,fm*ax1[0],fm*ax1[1],fm*ax1[2]);
	}
      }
    }

    if (limit) {
      dReal k = info->fps * stop_erp;
      info->c[row] = -k * limit_err;
      info->cfm[row] = stop_cfm;

      if (lostop == histop) {
	// limited low and high simultaneously
	info->lo[row] = -dInfinity;
	info->hi[row] = dInfinity;
      }
      else {
	if (limit == 1) {
	  // low limit
	  info->lo[row] = 0;
	  info->hi[row] = dInfinity;
	}
	else {
	  // high limit
	  info->lo[row] = -dInfinity;
	  info->hi[row] = 0;
	}

	// deal with bounce
	if (bounce > 0) {
	  // calculate joint velocity
	  dReal vel;
	  if (rotational) {
	    vel = dDOT(joint->node[0].body->avel,ax1);
	    if (joint->node[1].body)
	      vel -= dDOT(joint->node[1].body->avel,ax1);
	  }
	  else {
	    vel = dDOT(joint->node[0].body->lvel,ax1);
	    if (joint->node[1].body)
	      vel -= dDOT(joint->node[1].body->lvel,ax1);
	  }

	  // only apply bounce if the velocity is incoming, and if the
	  // resulting c[] exceeds what we already have.
	  if (limit == 1) {
	    // low limit
	    if (vel < 0) {
	      dReal newc = -bounce * vel;
	      if (newc > info->c[row]) info->c[row] = newc;
	    }
	  }
	  else {
	    // high limit - all those computations are reversed
	    if (vel > 0) {
	      dReal newc = -bounce * vel;
	      if (newc < info->c[row]) info->c[row] = newc;
	    }
	  }
	}
      }
    }
    return 1;
  }
  else return 0;
}

//****************************************************************************
// ball and socket

static void ballInit (dxJointBall *j)
{
  dSetZero (j->anchor1,4);
  dSetZero (j->anchor2,4);
}


static void ballGetInfo1 (dxJointBall *j, dxJoint::Info1 *info)
{
  info->m = 3;
  info->nub = 3;
}


static void ballGetInfo2 (dxJointBall *joint, dxJoint::Info2 *info)
{
  setBall (joint,info,joint->anchor1,joint->anchor2);
}


extern "C" void dJointSetBallAnchor (dxJointBall *joint,
				     dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dball_vtable,"joint is not a ball");
  setAnchors (joint,x,y,z,joint->anchor1,joint->anchor2);
}


extern "C" void dJointGetBallAnchor (dxJointBall *joint, dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__dball_vtable,"joint is not a ball");
  getAnchor (joint,result,joint->anchor1);
}


dxJoint::Vtable __dball_vtable = {
  sizeof(dxJointBall),
  (dxJoint::init_fn*) ballInit,
  (dxJoint::getInfo1_fn*) ballGetInfo1,
  (dxJoint::getInfo2_fn*) ballGetInfo2,
  dJointTypeBall};

//****************************************************************************
// hinge

static void hingeInit (dxJointHinge *j)
{
  dSetZero (j->anchor1,4);
  dSetZero (j->anchor2,4);
  dSetZero (j->axis1,4);
  j->axis1[0] = 1;
  dSetZero (j->axis2,4);
  j->axis2[0] = 1;
  dSetZero (j->qrel,4);
  j->limot.init (j->world);
}


static void hingeGetInfo1 (dxJointHinge *j, dxJoint::Info1 *info)
{
  info->nub = 5;    

  // see if joint is powered
  if (j->limot.fmax > 0)
    info->m = 6;	// powered hinge needs an extra constraint row
  else info->m = 5;

  // see if we're at a joint limit.
  if ((j->limot.lostop >= -M_PI || j->limot.histop <= M_PI) &&
       j->limot.lostop <= j->limot.histop) {
    dReal angle = getHingeAngle (j->node[0].body,j->node[1].body,j->axis1,
				 j->qrel);
    if (j->limot.testRotationalLimit (angle)) info->m = 6;
  }
}


static void hingeGetInfo2 (dxJointHinge *joint, dxJoint::Info2 *info)
{
  // set the three ball-and-socket rows
  setBall (joint,info,joint->anchor1,joint->anchor2);

  // set the two hinge rows. the hinge axis should be the only unconstrained
  // rotational axis, the angular velocity of the two bodies perpendicular to
  // the hinge axis should be equal. thus the constraint equations are
  //    p*w1 - p*w2 = 0
  //    q*w1 - q*w2 = 0
  // where p and q are unit vectors normal to the hinge axis, and w1 and w2
  // are the angular velocity vectors of the two bodies.

  dVector3 ax1;  // length 1 joint axis in global coordinates, from 1st body
  dVector3 p,q;  // plane space vectors for ax1
  dMULTIPLY0_331 (ax1,joint->node[0].body->R,joint->axis1);
  dPlaneSpace (ax1,p,q);

  int s3=3*info->rowskip;
  int s4=4*info->rowskip;

  info->J1a[s3+0] = p[0];
  info->J1a[s3+1] = p[1];
  info->J1a[s3+2] = p[2];
  info->J1a[s4+0] = q[0];
  info->J1a[s4+1] = q[1];
  info->J1a[s4+2] = q[2];

  if (joint->node[1].body) {
    info->J2a[s3+0] = -p[0];
    info->J2a[s3+1] = -p[1];
    info->J2a[s3+2] = -p[2];
    info->J2a[s4+0] = -q[0];
    info->J2a[s4+1] = -q[1];
    info->J2a[s4+2] = -q[2];
  }

  // compute the right hand side of the constraint equation. set relative
  // body velocities along p and q to bring the hinge back into alignment.
  // if ax1,ax2 are the unit length hinge axes as computed from body1 and
  // body2, we need to rotate both bodies along the axis u = (ax1 x ax2).
  // if `theta' is the angle between ax1 and ax2, we need an angular velocity
  // along u to cover angle erp*theta in one step :
  //   |angular_velocity| = angle/time = erp*theta / stepsize
  //                      = (erp*fps) * theta
  //    angular_velocity  = |angular_velocity| * (ax1 x ax2) / |ax1 x ax2|
  //                      = (erp*fps) * theta * (ax1 x ax2) / sin(theta)
  // ...as ax1 and ax2 are unit length. if theta is smallish,
  // theta ~= sin(theta), so
  //    angular_velocity  = (erp*fps) * (ax1 x ax2)
  // ax1 x ax2 is in the plane space of ax1, so we project the angular
  // velocity to p and q to find the right hand side.

  dVector3 ax2,b;
  if (joint->node[1].body) {
    dMULTIPLY0_331 (ax2,joint->node[1].body->R,joint->axis2);
  }
  else {
    ax2[0] = joint->axis2[0];
    ax2[1] = joint->axis2[1];
    ax2[2] = joint->axis2[2];
  }
  dCROSS (b,=,ax1,ax2);
  dReal k = info->fps * info->erp;
  info->c[3] = k * dDOT(b,p);
  info->c[4] = k * dDOT(b,q);

  // if the hinge is powered, or has joint limits, add in the stuff
  joint->limot.addLimot (joint,info,5,ax1,1);
}


// compute initial relative rotation body1 -> body2, or env -> body1

static void hingeComputeInitialRelativeRotation (dxJointHinge *joint)
{
  if (joint->node[0].body) {
    if (joint->node[1].body) {
      dQMultiply1 (joint->qrel,joint->node[0].body->q,joint->node[1].body->q);
    }
    else {
      // set joint->qrel to the transpose of the first body q
      joint->qrel[0] = joint->node[0].body->q[0];
      for (int i=1; i<4; i++) joint->qrel[i] = -joint->node[0].body->q[i];
    }
  }
}


extern "C" void dJointSetHingeAnchor (dxJointHinge *joint,
				      dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a hinge");
  setAnchors (joint,x,y,z,joint->anchor1,joint->anchor2);
  hingeComputeInitialRelativeRotation (joint);
}


extern "C" void dJointSetHingeAxis (dxJointHinge *joint,
				    dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a hinge");
  setAxes (joint,x,y,z,joint->axis1,joint->axis2);
  hingeComputeInitialRelativeRotation (joint);
}


extern "C" void dJointGetHingeAnchor (dxJointHinge *joint, dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a hinge");
  getAnchor (joint,result,joint->anchor1);
}


extern "C" void dJointGetHingeAxis (dxJointHinge *joint, dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a hinge");
  getAxis (joint,result,joint->axis1);
}


extern "C" void dJointSetHingeParam (dxJointHinge *joint,
				     int parameter, dReal value)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a hinge");
  joint->limot.set (parameter,value);
}


extern "C" dReal dJointGetHingeParam (dxJointHinge *joint, int parameter)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a hinge");
  return joint->limot.get (parameter);
}


extern "C" dReal dJointGetHingeAngle (dxJointHinge *joint)
{
  dAASSERT(joint);
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a hinge");
  if (joint->node[0].body) {
    return getHingeAngle (joint->node[0].body,joint->node[1].body,joint->axis1,
			  joint->qrel);
  }
  else return 0;
}


extern "C" dReal dJointGetHingeAngleRate (dxJointHinge *joint)
{
  dAASSERT(joint);
  dUASSERT(joint->vtable == &__dhinge_vtable,"joint is not a Hinge");
  if (joint->node[0].body) {
    dVector3 axis;
    dMULTIPLY0_331 (axis,joint->node[0].body->R,joint->axis1);
    dReal rate = dDOT(axis,joint->node[0].body->avel);
    if (joint->node[1].body) rate -= dDOT(axis,joint->node[1].body->avel);
    return rate;
  }
  else return 0;
}


dxJoint::Vtable __dhinge_vtable = {
  sizeof(dxJointHinge),
  (dxJoint::init_fn*) hingeInit,
  (dxJoint::getInfo1_fn*) hingeGetInfo1,
  (dxJoint::getInfo2_fn*) hingeGetInfo2,
  dJointTypeHinge};

//****************************************************************************
// slider

static void sliderInit (dxJointSlider *j)
{
  dSetZero (j->axis1,4);
  j->axis1[0] = 1;
  dSetZero (j->qrel,4);
  dSetZero (j->offset,4);
  j->limot.init (j->world);
}


extern "C" dReal dJointGetSliderPosition (dxJointSlider *joint)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dslider_vtable,"joint is not a slider");

  // get axis1 in global coordinates
  dVector3 ax1,q;
  dMULTIPLY0_331 (ax1,joint->node[0].body->R,joint->axis1);

  if (joint->node[1].body) {
    // get body2 + offset point in global coordinates
    dMULTIPLY0_331 (q,joint->node[1].body->R,joint->offset);
    for (int i=0; i<3; i++) q[i] = joint->node[0].body->pos[i] - q[i] - 
			      joint->node[1].body->pos[i];
  }
  else {
    for (int i=0; i<3; i++) q[i] = joint->node[0].body->pos[i] -
			      joint->offset[i];
			      
  }
  return dDOT(ax1,q);
}


extern "C" dReal dJointGetSliderPositionRate (dxJointSlider *joint)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dslider_vtable,"joint is not a slider");

  // get axis1 in global coordinates
  dVector3 ax1;
  dMULTIPLY0_331 (ax1,joint->node[0].body->R,joint->axis1);

  if (joint->node[1].body) {
    return dDOT(ax1,joint->node[0].body->lvel) -
      dDOT(ax1,joint->node[1].body->lvel);
  }
  else {
    return dDOT(ax1,joint->node[0].body->lvel);
  }
}


static void sliderGetInfo1 (dxJointSlider *j, dxJoint::Info1 *info)
{
  info->nub = 5;

  // see if joint is powered
  if (j->limot.fmax > 0)
    info->m = 6;	// powered slider needs an extra constraint row
  else info->m = 5;

  // see if we're at a joint limit.
  j->limot.limit = 0;
  if ((j->limot.lostop > -dInfinity || j->limot.histop < dInfinity) &&
      j->limot.lostop <= j->limot.histop) {
    // measure joint position
    dReal pos = dJointGetSliderPosition (j);
    if (pos <= j->limot.lostop) {
      j->limot.limit = 1;
      j->limot.limit_err = pos - j->limot.lostop;
      info->m = 6;
    }
    else if (pos >= j->limot.histop) {
      j->limot.limit = 2;
      j->limot.limit_err = pos - j->limot.histop;
      info->m = 6;
    }
  }
}


static void sliderGetInfo2 (dxJointSlider *joint, dxJoint::Info2 *info)
{
  int i,s = info->rowskip;
  int s2=2*s,s3=3*s,s4=4*s;

  // pull out pos and R for both bodies. also get the `connection'
  // vector pos2-pos1.

  dReal *pos1,*pos2,*R1,*R2;
  dVector3 c;
  pos1 = joint->node[0].body->pos;
  R1 = joint->node[0].body->R;
  if (joint->node[1].body) {
    pos2 = joint->node[1].body->pos;
    R2 = joint->node[1].body->R;
    for (i=0; i<3; i++) c[i] = pos2[i] - pos1[i];
  }
  else {
    pos2 = 0;
    R2 = 0;
  }

  // 3 rows to make body rotations equal
  info->J1a[0] = 1;
  info->J1a[s+1] = 1;
  info->J1a[s2+2] = 1;
  if (joint->node[1].body) {
    info->J2a[0] = -1;
    info->J2a[s+1] = -1;
    info->J2a[s2+2] = -1;
  }

  // remaining two rows. we want: vel2 = vel1 + w1 x c ... but this would
  // result in three equations, so we project along the planespace vectors
  // so that sliding along the slider axis is disregarded. for symmetry we
  // also substitute (w1+w2)/2 for w1, as w1 is supposed to equal w2.

  dVector3 ax1;	// joint axis in global coordinates (unit length)
  dVector3 p,q;	// plane space of ax1
  dMULTIPLY0_331 (ax1,R1,joint->axis1);
  dPlaneSpace (ax1,p,q);
  if (joint->node[1].body) {
    dVector3 tmp;
    dCROSS (tmp, = REAL(0.5) * ,c,p);
    for (i=0; i<3; i++) info->J2a[s3+i] = tmp[i];
    for (i=0; i<3; i++) info->J2a[s3+i] = tmp[i];
    dCROSS (tmp, = REAL(0.5) * ,c,q);
    for (i=0; i<3; i++) info->J2a[s4+i] = tmp[i];
    for (i=0; i<3; i++) info->J2a[s4+i] = tmp[i];
    for (i=0; i<3; i++) info->J2l[s3+i] = -p[i];
    for (i=0; i<3; i++) info->J2l[s4+i] = -q[i];
  }
  for (i=0; i<3; i++) info->J1l[s3+i] = p[i];
  for (i=0; i<3; i++) info->J1l[s4+i] = q[i];

  // compute the right hand side. the first three elements will result in
  // relative angular velocity of the two bodies - this is set to bring them
  // back into alignment. the correcting angular velocity is 
  //   |angular_velocity| = angle/time = erp*theta / stepsize
  //                      = (erp*fps) * theta
  //    angular_velocity  = |angular_velocity| * u
  //                      = (erp*fps) * theta * u
  // where rotation along unit length axis u by theta brings body 2's frame
  // to qrel with respect to body 1's frame. using a small angle approximation
  // for sin(), this gives
  //    angular_velocity  = (erp*fps) * 2 * v
  // where the quaternion of the relative rotation between the two bodies is
  //    q = [cos(theta/2) sin(theta/2)*u] = [s v]

  // get qerr = relative rotation (rotation error) between two bodies
  dQuaternion qerr,e;
  if (joint->node[1].body) {
    dQuaternion qq;
    dQMultiply1 (qq,joint->node[0].body->q,joint->node[1].body->q);
    dQMultiply2 (qerr,qq,joint->qrel);
  }
  else {
    dQMultiply3 (qerr,joint->node[0].body->q,joint->qrel);
  }
  if (qerr[0] < 0) {
    qerr[1] = -qerr[1];		// adjust sign of qerr to make theta small
    qerr[2] = -qerr[2];
    qerr[3] = -qerr[3];
  }
  dMULTIPLY0_331 (e,joint->node[0].body->R,qerr+1); // @@@ bad SIMD padding!
  dReal k = info->fps * info->erp;
  info->c[0] = 2*k * e[0];
  info->c[1] = 2*k * e[1];
  info->c[2] = 2*k * e[2];

  // compute last two elements of right hand side. we want to align the offset
  // point (in body 2's frame) with the center of body 1.
  if (joint->node[1].body) {
    dVector3 ofs;		// offset point in global coordinates
    dMULTIPLY0_331 (ofs,R2,joint->offset);
    for (i=0; i<3; i++) c[i] += ofs[i];
    info->c[3] = k * dDOT(p,c);
    info->c[4] = k * dDOT(q,c);
  }
  else {
    dVector3 ofs;		// offset point in global coordinates
    for (i=0; i<3; i++) ofs[i] = joint->offset[i] - pos1[i];
    info->c[3] = k * dDOT(p,ofs);
    info->c[4] = k * dDOT(q,ofs);
  }

  // if the slider is powered, or has joint limits, add in the extra row
  joint->limot.addLimot (joint,info,5,ax1,0);
}


extern "C" void dJointSetSliderAxis (dxJointSlider *joint,
				     dReal x, dReal y, dReal z)
{
  int i;
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dslider_vtable,"joint is not a slider");
  setAxes (joint,x,y,z,joint->axis1,0);

  // compute initial relative rotation body1 -> body2, or env -> body1
  // also compute center of body1 w.r.t body 2
  if (joint->node[1].body) {
    dQMultiply1 (joint->qrel,joint->node[0].body->q,joint->node[1].body->q);
    dVector3 c;
    for (i=0; i<3; i++)
      c[i] = joint->node[0].body->pos[i] - joint->node[1].body->pos[i];
    dMULTIPLY1_331 (joint->offset,joint->node[1].body->R,c);
  }
  else {
    // set joint->qrel to the transpose of the first body's q
    joint->qrel[0] = joint->node[0].body->q[0];
    for (i=1; i<4; i++) joint->qrel[i] = -joint->node[0].body->q[i];
    for (i=0; i<3; i++) joint->offset[i] = joint->node[0].body->pos[i];
  }
}


extern "C" void dJointGetSliderAxis (dxJointSlider *joint, dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__dslider_vtable,"joint is not a slider");
  getAxis (joint,result,joint->axis1);
}


extern "C" void dJointSetSliderParam (dxJointSlider *joint,
				      int parameter, dReal value)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dslider_vtable,"joint is not a slider");
  joint->limot.set (parameter,value);
}


extern "C" dReal dJointGetSliderParam (dxJointSlider *joint, int parameter)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dslider_vtable,"joint is not a slider");
  return joint->limot.get (parameter);
}


dxJoint::Vtable __dslider_vtable = {
  sizeof(dxJointSlider),
  (dxJoint::init_fn*) sliderInit,
  (dxJoint::getInfo1_fn*) sliderGetInfo1,
  (dxJoint::getInfo2_fn*) sliderGetInfo2,
  dJointTypeSlider};

//****************************************************************************
// contact

static void contactInit (dxJointContact *j)
{
  // default frictionless contact. hmmm, this info gets overwritten straight
  // away anyway, so why bother?
  j->contact.surface.mode = 0;
  j->contact.surface.mu = 0;
  dSetZero (j->contact.geom.pos,4);
  dSetZero (j->contact.geom.normal,4);
  j->contact.geom.depth = 0;
}


static void contactGetInfo1 (dxJointContact *j, dxJoint::Info1 *info)
{
  // make sure mu's >= 0, then calculate number of constraint rows and number
  // of unbounded rows.
  int m = 1, nub=0;
  if (j->contact.surface.mu < 0) j->contact.surface.mu = 0;
  if (j->contact.surface.mode & dContactMu2) {
    if (j->contact.surface.mu > 0) m++;
    if (j->contact.surface.mu2 < 0) j->contact.surface.mu2 = 0;
    if (j->contact.surface.mu2 > 0) m++;
    if (j->contact.surface.mu  == dInfinity) nub ++;
    if (j->contact.surface.mu2 == dInfinity) nub ++;
  }
  else {
    if (j->contact.surface.mu > 0) m += 2;
    if (j->contact.surface.mu == dInfinity) nub += 2;
  }

  j->the_m = m;
  info->m = m;
  info->nub = nub;
}


static void contactGetInfo2 (dxJointContact *j, dxJoint::Info2 *info)
{
  int i,s = info->rowskip;
  int s2 = 2*s;

  // get normal, with sign adjusted for body1/body2 polarity
  dVector3 normal;
  if (j->flags & dJOINT_REVERSE) {
    normal[0] = j->contact.geom.normal[0];
    normal[1] = j->contact.geom.normal[1];
    normal[2] = j->contact.geom.normal[2];
  }
  else {
    normal[0] = - j->contact.geom.normal[0];
    normal[1] = - j->contact.geom.normal[1];
    normal[2] = - j->contact.geom.normal[2];
  }
  normal[3] = 0;	// @@@ hmmm

  // c1,c2 = contact points with respect to body PORs
  dVector3 c1,c2;
  for (i=0; i<3; i++) c1[i] = j->contact.geom.pos[i] - j->node[0].body->pos[i];

  // set jacobian for normal
  info->J1l[0] = normal[0];
  info->J1l[1] = normal[1];
  info->J1l[2] = normal[2];
  dCROSS (info->J1a,=,c1,normal);
  if (j->node[1].body) {
    for (i=0; i<3; i++) c2[i] = j->contact.geom.pos[i] -
			  j->node[1].body->pos[i];
    info->J2l[0] = -normal[0];
    info->J2l[1] = -normal[1];
    info->J2l[2] = -normal[2];
    dCROSS (info->J2a,= -,c2,normal);
  }

  // set right hand side and cfm value for normal
  dReal erp = info->erp;
  if (j->contact.surface.mode & dContactSoftERP)
    erp = j->contact.surface.soft_erp;
  dReal k = info->fps * erp;
  info->c[0] = k*j->contact.geom.depth;
  if (j->contact.surface.mode & dContactSoftCFM)
    info->cfm[0] = j->contact.surface.soft_cfm;

  // deal with bounce
  if (j->contact.surface.mode & dContactBounce) {
    // calculate outgoing velocity (-ve for incoming contact)
    dReal outgoing = dDOT(info->J1l,j->node[0].body->lvel) +
      dDOT(info->J1a,j->node[0].body->avel);
    if (j->node[1].body) {
      outgoing += dDOT(info->J2l,j->node[1].body->lvel) +
	dDOT(info->J2a,j->node[1].body->avel);
    }
    // only apply bounce if the outgoing velocity is greater than the
    // threshold, and if the resulting c[0] exceeds what we already have.
    if (j->contact.surface.bounce_vel >= 0 &&
	(-outgoing) > j->contact.surface.bounce_vel) {
      dReal newc = - j->contact.surface.bounce * outgoing;
      if (newc > info->c[0]) info->c[0] = newc;
    }
  }

  // set LCP limits for normal
  info->lo[0] = 0;
  info->hi[0] = dInfinity;

  // now do jacobian for tangential forces
  dVector3 t1,t2;	// two vectors tangential to normal

  // first friction direction
  if (j->the_m >= 2) {
    if (j->contact.surface.mode & dContactFDir1) {	// use fdir1 ?
      t1[0] = j->contact.fdir1[0];
      t1[1] = j->contact.fdir1[1];
      t1[2] = j->contact.fdir1[2];
      dCROSS (t2,=,normal,t1);
    }
    else {
      dPlaneSpace (normal,t1,t2);
    }
    info->J1l[s+0] = t1[0];
    info->J1l[s+1] = t1[1];
    info->J1l[s+2] = t1[2];
    dCROSS (info->J1a+s,=,c1,t1);
    if (j->node[1].body) {
      info->J2l[s+0] = -t1[0];
      info->J2l[s+1] = -t1[1];
      info->J2l[s+2] = -t1[2];
      dCROSS (info->J2a+s,= -,c2,t1);
    }
    // set right hand side
    if (j->contact.surface.mode & dContactMotion1) {
      info->c[1] = j->contact.surface.motion1;
    }
    // set LCP bounds and friction index. this depends on the approximation
    // mode
    info->lo[1] = -j->contact.surface.mu;
    info->hi[1] = j->contact.surface.mu;
    if (j->contact.surface.mode & dContactApprox1_1) info->findex[1] = 0;

    // set slip (constraint force mixing)
    if (j->contact.surface.mode & dContactSlip1)
      info->cfm[1] = j->contact.surface.slip1;
  }

  // second friction direction
  if (j->the_m >= 3) {
    info->J1l[s2+0] = t2[0];
    info->J1l[s2+1] = t2[1];
    info->J1l[s2+2] = t2[2];
    dCROSS (info->J1a+s2,=,c1,t2);
    if (j->node[1].body) {
      info->J2l[s2+0] = -t2[0];
      info->J2l[s2+1] = -t2[1];
      info->J2l[s2+2] = -t2[2];
      dCROSS (info->J2a+s2,= -,c2,t2);
    }
    // set right hand side
    if (j->contact.surface.mode & dContactMotion2) {
      info->c[2] = j->contact.surface.motion2;
    }
    // set LCP bounds and friction index. this depends on the approximation
    // mode
    if (j->contact.surface.mode & dContactMu2) {
      info->lo[2] = -j->contact.surface.mu2;
      info->hi[2] = j->contact.surface.mu2;
    }
    else {
      info->lo[2] = -j->contact.surface.mu;
      info->hi[2] = j->contact.surface.mu;
    }
    if (j->contact.surface.mode & dContactApprox1_2) info->findex[2] = 0;

    // set slip (constraint force mixing)
    if (j->contact.surface.mode & dContactSlip2)
      info->cfm[2] = j->contact.surface.slip2;
  }
}


dxJoint::Vtable __dcontact_vtable = {
  sizeof(dxJointContact),
  (dxJoint::init_fn*) contactInit,
  (dxJoint::getInfo1_fn*) contactGetInfo1,
  (dxJoint::getInfo2_fn*) contactGetInfo2,
  dJointTypeContact};

//****************************************************************************
// hinge 2. note that this joint must be attached to two bodies for it to work

static dReal measureHinge2Angle (dxJointHinge2 *joint)
{
  dVector3 a1,a2;
  dMULTIPLY0_331 (a1,joint->node[1].body->R,joint->axis2);
  dMULTIPLY1_331 (a2,joint->node[0].body->R,a1);
  dReal x = dDOT(joint->v1,a2);
  dReal y = dDOT(joint->v2,a2);
  return -dAtan2 (y,x);
}


static void hinge2Init (dxJointHinge2 *j)
{
  dSetZero (j->anchor1,4);
  dSetZero (j->anchor2,4);
  dSetZero (j->axis1,4);
  j->axis1[0] = 1;
  dSetZero (j->axis2,4);
  j->axis2[1] = 1;
  j->c0 = 0;
  j->s0 = 0;

  dSetZero (j->v1,4);
  j->v1[0] = 1;
  dSetZero (j->v2,4);
  j->v2[1] = 1;

  j->limot1.init (j->world);
  j->limot2.init (j->world);

  j->susp_erp = j->world->global_erp;
  j->susp_cfm = j->world->global_cfm;

  j->flags |= dJOINT_TWOBODIES;
}


static void hinge2GetInfo1 (dxJointHinge2 *j, dxJoint::Info1 *info)
{
  info->m = 4;
  info->nub = 4;

  // see if we're powered or at a joint limit for axis 1
  int atlimit=0;
  if ((j->limot1.lostop >= -M_PI || j->limot1.histop <= M_PI) &&
      j->limot1.lostop <= j->limot1.histop) {
    dReal angle = measureHinge2Angle (j);
    if (j->limot1.testRotationalLimit (angle)) atlimit = 1;
  }
  if (atlimit || j->limot1.fmax > 0) info->m++;

  // see if we're powering axis 2 (we currently never limit this axis)
  j->limot2.limit = 0;
  if (j->limot2.fmax > 0) info->m++;
}


// macro that computes ax1,ax2 = axis 1 and 2 in global coordinates (they are
// relative to body 1 and 2 initially) and then computes the constrained
// rotational axis as the cross product of ax1 and ax2.
// the sin and cos of the angle between axis 1 and 2 is computed, this comes
// from dot and cross product rules.

#define HINGE2_GET_AXIS_INFO(axis,sin_angle,cos_angle) \
  dVector3 ax1,ax2; \
  dMULTIPLY0_331 (ax1,joint->node[0].body->R,joint->axis1); \
  dMULTIPLY0_331 (ax2,joint->node[1].body->R,joint->axis2); \
  dCROSS (axis,=,ax1,ax2); \
  sin_angle = dSqrt (axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]); \
  cos_angle = dDOT (ax1,ax2);


static void hinge2GetInfo2 (dxJointHinge2 *joint, dxJoint::Info2 *info)
{
  // get information we need to set the hinge row
  dReal s,c;
  dVector3 q;
  HINGE2_GET_AXIS_INFO (q,s,c);
  dNormalize3 (q);		// @@@ quicker: divide q by s ?

  // set the three ball-and-socket rows (aligned to the suspension axis ax1)
  setBall2 (joint,info,joint->anchor1,joint->anchor2,ax1,joint->susp_erp);

  // set the hinge row
  int s3=3*info->rowskip;
  info->J1a[s3+0] = q[0];
  info->J1a[s3+1] = q[1];
  info->J1a[s3+2] = q[2];
  if (joint->node[1].body) {
    info->J2a[s3+0] = -q[0];
    info->J2a[s3+1] = -q[1];
    info->J2a[s3+2] = -q[2];
  }

  // compute the right hand side for the constrained rotational DOF.
  // axis 1 and axis 2 are separated by an angle `theta'. the desired
  // separation angle is theta0. sin(theta0) and cos(theta0) are recorded
  // in the joint structure. the correcting angular velocity is:
  //   |angular_velocity| = angle/time = erp*(theta0-theta) / stepsize
  //                      = (erp*fps) * (theta0-theta)
  // (theta0-theta) can be computed using the following small-angle-difference
  // approximation:
  //   theta0-theta ~= tan(theta0-theta)
  //                 = sin(theta0-theta)/cos(theta0-theta)
  //                 = (c*s0 - s*c0) / (c*c0 + s*s0)
  //                 = c*s0 - s*c0         assuming c*c0 + s*s0 ~= 1
  // where c = cos(theta), s = sin(theta)
  //       c0 = cos(theta0), s0 = sin(theta0)

  dReal k = info->fps * info->erp;
  info->c[3] = k * (joint->c0 * s - joint->s0 * c);

  // if the axis1 hinge is powered, or has joint limits, add in more stuff
  int row = 4 + joint->limot1.addLimot (joint,info,4,ax1,1);

  // if the axis2 hinge is powered, add in more stuff
  joint->limot2.addLimot (joint,info,row,ax2,1);

  // set parameter for the suspension
  info->cfm[0] = joint->susp_cfm;
}


// compute vectors v1 and v2 (embedded in body1), used to measure angle
// between body 1 and body 2

static void makeHinge2V1andV2 (dxJointHinge2 *joint)
{
  if (joint->node[0].body) {
    // get axis 1 and 2 in global coords
    dVector3 ax1,ax2,v;
    dMULTIPLY0_331 (ax1,joint->node[0].body->R,joint->axis1);
    dMULTIPLY0_331 (ax2,joint->node[1].body->R,joint->axis2);

    // don't do anything if the axis1 or axis2 vectors are zero or the same
    if ((ax1[0]==0 && ax1[1]==0 && ax1[2]==0) ||
	(ax2[0]==0 && ax2[1]==0 && ax2[2]==0) ||
	(ax1[0]==ax2[0] && ax1[1]==ax2[1] && ax1[2]==ax2[2])) return;

    // modify axis 2 so it's perpendicular to axis 1
    dReal k = dDOT(ax1,ax2);
    for (int i=0; i<3; i++) ax2[i] -= k*ax1[i];
    dNormalize3 (ax2);

    // make v1 = modified axis2, v2 = axis1 x (modified axis2)
    dCROSS (v,=,ax1,ax2);
    dMULTIPLY1_331 (joint->v1,joint->node[0].body->R,ax2);
    dMULTIPLY1_331 (joint->v2,joint->node[0].body->R,v);
  }
}


extern "C" void dJointSetHinge2Anchor (dxJointHinge2 *joint,
				       dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  setAnchors (joint,x,y,z,joint->anchor1,joint->anchor2);
  makeHinge2V1andV2 (joint);
}


extern "C" void dJointSetHinge2Axis1 (dxJointHinge2 *joint,
				      dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if (joint->node[0].body) {
    dReal q[4];
    q[0] = x;
    q[1] = y;
    q[2] = z;
    q[3] = 0;
    dNormalize3 (q);
    dMULTIPLY1_331 (joint->axis1,joint->node[0].body->R,q);
    joint->axis1[3] = 0;

    // compute the sin and cos of the angle between axis 1 and axis 2
    dVector3 ax;
    HINGE2_GET_AXIS_INFO(ax,joint->s0,joint->c0);
  }
  makeHinge2V1andV2 (joint);
}


extern "C" void dJointSetHinge2Axis2 (dxJointHinge2 *joint,
				      dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if (joint->node[1].body) {
    dReal q[4];
    q[0] = x;
    q[1] = y;
    q[2] = z;
    q[3] = 0;
    dNormalize3 (q);
    dMULTIPLY1_331 (joint->axis2,joint->node[1].body->R,q);
    joint->axis1[3] = 0;

    // compute the sin and cos of the angle between axis 1 and axis 2
    dVector3 ax;
    HINGE2_GET_AXIS_INFO(ax,joint->s0,joint->c0);
  }
  makeHinge2V1andV2 (joint);
}


extern "C" void dJointSetHinge2Param (dxJointHinge2 *joint,
				      int parameter, dReal value)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if ((parameter & 0xff00) == 0x100) {
    joint->limot2.set (parameter & 0xff,value);
  }
  else {
    if (parameter == dParamSuspensionERP) joint->susp_erp = value;
    else if (parameter == dParamSuspensionCFM) joint->susp_cfm = value;
    else joint->limot1.set (parameter,value);
  }
}


extern "C" void dJointGetHinge2Anchor (dxJointHinge2 *joint, dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  getAnchor (joint,result,joint->anchor1);
}


extern "C" void dJointGetHinge2Axis1 (dxJointHinge2 *joint, dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if (joint->node[0].body) {
    dMULTIPLY0_331 (result,joint->node[0].body->R,joint->axis1);
  }
}


extern "C" void dJointGetHinge2Axis2 (dxJointHinge2 *joint, dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if (joint->node[1].body) {
    dMULTIPLY0_331 (result,joint->node[1].body->R,joint->axis2);
  }
}


extern "C" dReal dJointGetHinge2Param (dxJointHinge2 *joint, int parameter)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if ((parameter & 0xff00) == 0x100) {
    return joint->limot2.get (parameter & 0xff);
  }
  else {
    if (parameter == dParamSuspensionERP) return joint->susp_erp;
    else if (parameter == dParamSuspensionCFM) return joint->susp_cfm;
    else return joint->limot1.get (parameter);
  }
}


extern "C" dReal dJointGetHinge2Angle1 (dxJointHinge2 *joint)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if (joint->node[0].body) return measureHinge2Angle (joint);
  else return 0;
}


extern "C" dReal dJointGetHinge2Angle1Rate (dxJointHinge2 *joint)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if (joint->node[0].body) {
    dVector3 axis;
    dMULTIPLY0_331 (axis,joint->node[0].body->R,joint->axis1);
    dReal rate = dDOT(axis,joint->node[0].body->avel);
    if (joint->node[1].body) rate -= dDOT(axis,joint->node[1].body->avel);
    return rate;
  }
  else return 0;
}


extern "C" dReal dJointGetHinge2Angle2Rate (dxJointHinge2 *joint)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dhinge2_vtable,"joint is not a hinge2");
  if (joint->node[0].body && joint->node[1].body) {
    dVector3 axis;
    dMULTIPLY0_331 (axis,joint->node[1].body->R,joint->axis2);
    dReal rate = dDOT(axis,joint->node[0].body->avel);
    if (joint->node[1].body) rate -= dDOT(axis,joint->node[1].body->avel);
    return rate;
  }
  else return 0;
}


dxJoint::Vtable __dhinge2_vtable = {
  sizeof(dxJointHinge2),
  (dxJoint::init_fn*) hinge2Init,
  (dxJoint::getInfo1_fn*) hinge2GetInfo1,
  (dxJoint::getInfo2_fn*) hinge2GetInfo2,
  dJointTypeHinge2};

//****************************************************************************
// universal

static void universalInit (dxJointUniversal *j)
{
  dSetZero (j->anchor1,4);
  dSetZero (j->anchor2,4);
  dSetZero (j->axis1,4);
  j->axis1[0] = 1;
  dSetZero (j->axis2,4);
  j->axis2[1] = 1;
}


static void universalGetInfo1 (dxJointUniversal *j, dxJoint::Info1 *info)
{
  info->nub = 4;
  info->m = 4;
}


static void universalGetInfo2 (dxJointUniversal *joint, dxJoint::Info2 *info)
{
  // set the three ball-and-socket rows
  setBall (joint,info,joint->anchor1,joint->anchor2);

  // set the universal joint row. the angular velocity about an axis
  // perpendicular to both joint axes should be equal. thus the constraint
  // equation is
  //    p*w1 - p*w2 = 0
  // where p is a vector normal to both joint axes, and w1 and w2
  // are the angular velocity vectors of the two bodies.

  // length 1 joint axis in global coordinates, from each body
  dVector3 ax1, ax2;
  // length 1 vector perpendicular to ax1 and ax2. Neither body can rotate
  // about this.
  dVector3 p;
  
  // This says "ax1 = joint->node[0].body->R * joint->axis1"
  dMULTIPLY0_331 (ax1,joint->node[0].body->R,joint->axis1);
  if (joint->node[1].body) {
    dMULTIPLY0_331 (ax2,joint->node[1].body->R,joint->axis2);
  }
  else {
    ax2[0] = joint->axis2[0];
    ax2[1] = joint->axis2[1];
    ax2[2] = joint->axis2[2];
  }

  // if ax1 and ax2 are almost parallel, p won't be perpendicular to them.
  // Is there some more robust way to do this?
  dCROSS(p, =, ax1, ax2);
  dNormalize3(p);
 
  int s3=3*info->rowskip;

  info->J1a[s3+0] = p[0];
  info->J1a[s3+1] = p[1];
  info->J1a[s3+2] = p[2];

  if (joint->node[1].body) {
    info->J2a[s3+0] = -p[0];
    info->J2a[s3+1] = -p[1];
    info->J2a[s3+2] = -p[2];
  }

  // compute the right hand side of the constraint equation. set relative
  // body velocities along p to bring the axes back to perpendicular.
  // If ax1, ax2 are unit length joint axes as computed from body1 and
  // body2, we need to rotate both bodies along the axis p.  If theta
  // is the angle between ax1 and ax2, we need an angular velocity
  // along p to cover the angle erp * (theta - Pi/2) in one step:
  //
  //   |angular_velocity| = angle/time = erp*(theta - Pi/2) / stepsize
  //                      = (erp*fps) * (theta - Pi/2)
  //
  // if theta is close to Pi/2, 
  // theta - Pi/2 ~= cos(theta), so
  //    |angular_velocity|  = (erp*fps) * (ax1 dot ax2)

  info->c[3] = info->fps * info->erp * - dDOT(ax1, ax2);
}


extern "C" void dJointSetUniversalAnchor (dxJointUniversal *joint,
					  dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__duniversal_vtable,"joint is not a universal");
  setAnchors (joint,x,y,z,joint->anchor1,joint->anchor2);
}


extern "C" void dJointSetUniversalAxis1 (dxJointUniversal *joint,
					 dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__duniversal_vtable,"joint is not a universal");
  if (joint->node[0].body) {
    dReal q[4];
    q[0] = x;
    q[1] = y;
    q[2] = z;
    q[3] = 0;
    dNormalize3 (q);
    dMULTIPLY1_331 (joint->axis1,joint->node[0].body->R,q);
  }
  joint->axis1[3] = 0;
}


extern "C" void dJointSetUniversalAxis2 (dxJointUniversal *joint,
					 dReal x, dReal y, dReal z)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__duniversal_vtable,"joint is not a universal");
  if (joint->node[1].body) {
    dReal q[4];
    q[0] = x;
    q[1] = y;
    q[2] = z;
    q[3] = 0;
    dNormalize3 (q);
    dMULTIPLY1_331 (joint->axis2,joint->node[1].body->R,q);
  }
  joint->axis2[3] = 0;
}


extern "C" void dJointGetUniversalAnchor (dxJointUniversal *joint,
					  dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__duniversal_vtable,"joint is not a universal");
  getAnchor (joint,result,joint->anchor1);
}


extern "C" void dJointGetUniversalAxis1 (dxJointUniversal *joint,
					 dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__duniversal_vtable,"joint is not a universal");
  if (joint->node[0].body) {
    dMULTIPLY0_331 (result, joint->node[0].body->R, joint->axis1);
  }
}


extern "C" void dJointGetUniversalAxis2 (dxJointUniversal *joint,
					 dVector3 result)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
  dUASSERT(joint->vtable == &__duniversal_vtable,"joint is not a universal");
  if (joint->node[1].body) {
    dMULTIPLY0_331 (result, joint->node[1].body->R, joint->axis2);
  }
}


dxJoint::Vtable __duniversal_vtable = {
  sizeof(dxJointUniversal),
  (dxJoint::init_fn*) universalInit,
  (dxJoint::getInfo1_fn*) universalGetInfo1,
  (dxJoint::getInfo2_fn*) universalGetInfo2,
  dJointTypeUniversal};

//****************************************************************************
// angular motor

static void amotorInit (dxJointAMotor *j)
{
  int i;
  j->num = 0;
  j->mode = dAMotorUser;
  for (i=0; i<3; i++) {
    j->rel[i] = 0;
    dSetZero (j->axis[i],4);
    j->limot[i].init (j->world);
    j->angle[i] = 0;
  }
  dSetZero (j->reference1,4);
  dSetZero (j->reference2,4);

  j->flags |= dJOINT_TWOBODIES;
}


// compute the 3 axes in global coordinates

static void amotorComputeGlobalAxes (dxJointAMotor *joint, dVector3 ax[3])
{
  if (joint->mode == dAMotorEuler) {
    // special handling for euler mode
    dMULTIPLY0_331 (ax[0],joint->node[0].body->R,joint->axis[0]);
    dMULTIPLY0_331 (ax[2],joint->node[1].body->R,joint->axis[2]);
    dCROSS (ax[1],=,ax[2],ax[0]);
    dNormalize3 (ax[1]);
  }
  else {
    for (int i=0; i < joint->num; i++) {
      if (joint->rel[i] == 1) {
	// relative to b1
	dMULTIPLY0_331 (ax[i],joint->node[0].body->R,joint->axis[i]);
      }
      if (joint->rel[i] == 2) {
	// relative to b2
	dMULTIPLY0_331 (ax[i],joint->node[1].body->R,joint->axis[i]);
      }
      else {
	// global - just copy it
	ax[i][0] = joint->axis[i][0];
	ax[i][1] = joint->axis[i][1];
	ax[i][2] = joint->axis[i][2];
      }
    }
  }
}


static void amotorComputeEulerAngles (dxJointAMotor *joint, dVector3 ax[3])
{
  // assumptions:
  //   global axes already calculated --> ax
  //   axis[0] is relative to body 1 --> global ax[0]
  //   axis[2] is relative to body 2 --> global ax[2]
  //   ax[1] = ax[2] x ax[0]
  //   original ax[0] and ax[2] are perpendicular
  //   reference1 is perpendicular to ax[0] (in body 1 frame)
  //   reference2 is perpendicular to ax[2] (in body 2 frame)
  //   all ax[] and reference vectors are unit length

  // calculate references in global frame
  dVector3 ref1,ref2;
  dMULTIPLY0_331 (ref1,joint->node[0].body->R,joint->reference1);
  dMULTIPLY0_331 (ref2,joint->node[1].body->R,joint->reference2);

  // get q perpendicular to both ax[0] and ref1, get first euler angle
  dVector3 q;
  dCROSS (q,=,ax[0],ref1);
  joint->angle[0] = -dAtan2 (dDOT(ax[2],q),dDOT(ax[2],ref1));

  // get q perpendicular to both ax[0] and ax[1], get second euler angle
  dCROSS (q,=,ax[0],ax[1]);
  joint->angle[1] = -dAtan2 (dDOT(ax[2],ax[0]),dDOT(ax[2],q));

  // get q perpendicular to both ax[1] and ax[2], get third euler angle
  dCROSS (q,=,ax[1],ax[2]);
  joint->angle[2] = -dAtan2 (dDOT(ref2,ax[1]), dDOT(ref2,q));
}


// set the reference vectors as follows:
//   * reference1 = current axis[2] relative to body 1
//   * reference2 = current axis[0] relative to body 2
// this assumes that:
//    * axis[0] is relative to body 1
//    * axis[2] is relative to body 2

static void amotorSetEulerReferenceVectors (dxJointAMotor *j)
{
  if (j->node[0].body && j->node[1].body) {
    dVector3 r;		// axis[2] and axis[0] in global coordinates
    dMULTIPLY0_331 (r,j->node[1].body->R,j->axis[2]);
    dMULTIPLY1_331 (j->reference1,j->node[0].body->R,r);
    dMULTIPLY0_331 (r,j->node[0].body->R,j->axis[0]);
    dMULTIPLY1_331 (j->reference2,j->node[1].body->R,r);
  }
}


static void amotorGetInfo1 (dxJointAMotor *j, dxJoint::Info1 *info)
{
  info->m = 0;
  info->nub = 0;

  // compute the axes and angles, if in euler mode
  if (j->mode == dAMotorEuler) {
    dVector3 ax[3];
    amotorComputeGlobalAxes (j,ax);
    amotorComputeEulerAngles (j,ax);
  }

  // see if we're powered or at a joint limit for each axis
  for (int i=0; i < j->num; i++) {
    if (j->limot[i].testRotationalLimit (j->angle[i]) ||
	j->limot[i].fmax > 0) {
      info->m++;
    }
  }
}


static void amotorGetInfo2 (dxJointAMotor *joint, dxJoint::Info2 *info)
{
  int i;

  // compute the axes (if not global)
  dVector3 ax[3];
  amotorComputeGlobalAxes (joint,ax);

  // in euler angle mode we do not actually constrain the angular velocity
  // along the axes axis[0] and axis[2] (although we do use axis[1]) :
  //
  //    to get			constrain w2-w1 along		...not
  //    ------			---------------------		------
  //    d(angle[0])/dt = 0	ax[1] x ax[2]			ax[0]
  //    d(angle[1])/dt = 0	ax[1]
  //    d(angle[2])/dt = 0	ax[0] x ax[1]			ax[2]
  //
  // constraining w2-w1 along an axis 'a' means that a'*(w2-w1)=0.
  // to prove the result for angle[0], write the expression for angle[0] from
  // GetInfo1 then take the derivative. to prove this for angle[2] it is
  // easier to take the euler rate expression for d(angle[2])/dt with respect
  // to the components of w and set that to 0.

  dVector3 *axptr[3];
  axptr[0] = &ax[0];
  axptr[1] = &ax[1];
  axptr[2] = &ax[2];

  dVector3 ax0_cross_ax1;
  dVector3 ax1_cross_ax2;
  if (joint->mode == dAMotorEuler) {
    dCROSS (ax0_cross_ax1,=,ax[0],ax[1]);
    axptr[2] = &ax0_cross_ax1;
    dCROSS (ax1_cross_ax2,=,ax[1],ax[2]);
    axptr[0] = &ax1_cross_ax2;
  }

  int row=0;
  for (i=0; i < joint->num; i++) {
    row += joint->limot[i].addLimot (joint,info,row,*(axptr[i]),1);
  }
}


extern "C" void dJointSetAMotorNumAxes (dxJointAMotor *joint, int num)
{
  dAASSERT(joint && num >= 0 && num <= 3);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  if (joint->mode == dAMotorEuler) {
    joint->num = 3;
  }
  else {
    if (num < 0) num = 0;
    if (num > 3) num = 3;
    joint->num = num;
  }
}


extern "C" void dJointSetAMotorAxis (dxJointAMotor *joint, int anum, int rel,
				     dReal x, dReal y, dReal z)
{
  dAASSERT(joint && anum >= 0 && anum <= 2 && rel >= 0 && rel <= 2);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  if (anum < 0) anum = 0;
  if (anum > 2) anum = 2;
  joint->rel[anum] = rel;

  // x,y,z is always in global coordinates regardless of rel, so we may have
  // to convert it to be relative to a body
  dVector3 r;
  r[0] = x;
  r[1] = y;
  r[2] = z;
  r[3] = 0;
  if (rel > 0) {
    if (rel==1) {
      dMULTIPLY1_331 (joint->axis[anum],joint->node[0].body->R,r);
    }
    else {
      dMULTIPLY1_331 (joint->axis[anum],joint->node[1].body->R,r);
    }
  }
  else {
    joint->axis[anum][0] = r[0];
    joint->axis[anum][1] = r[1];
    joint->axis[anum][2] = r[2];
  }
  dNormalize3 (joint->axis[anum]);
  if (joint->mode == dAMotorEuler) amotorSetEulerReferenceVectors (joint);
}


extern "C" void dJointSetAMotorAngle (dxJointAMotor *joint, int anum,
				      dReal angle)
{
  dAASSERT(joint && anum >= 0 && anum < 3);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  if (joint->mode == dAMotorUser) {
    if (anum < 0) anum = 0;
    if (anum > 3) anum = 3;
    joint->angle[anum] = angle;
  }
}


extern "C" void dJointSetAMotorParam (dxJointAMotor *joint, int parameter,
				      dReal value)
{
  dAASSERT(joint);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  int anum = parameter >> 8;
  if (anum < 0) anum = 0;
  if (anum > 2) anum = 2;
  parameter &= 0xff;
  joint->limot[anum].set (parameter, value);
}


extern "C" void dJointSetAMotorMode (dxJointAMotor *joint, int mode)
{
  dAASSERT(joint);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  joint->mode = mode;
  if (joint->mode == dAMotorEuler) {
    joint->num = 3;
    amotorSetEulerReferenceVectors (joint);
  }
}


extern "C" int dJointGetAMotorNumAxes (dxJointAMotor *joint)
{
  dAASSERT(joint);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  return joint->num;
}


extern "C" void dJointGetAMotorAxis (dxJointAMotor *joint, int anum,
				     dVector3 result)
{
  dAASSERT(joint && anum >= 0 && anum < 3);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  if (anum < 0) anum = 0;
  if (anum > 2) anum = 2;
  if (joint->rel[anum] > 0) {
    if (joint->rel[anum]==1) {
      dMULTIPLY0_331 (result,joint->node[0].body->R,joint->axis[anum]);
    }
    else {
      dMULTIPLY0_331 (result,joint->node[1].body->R,joint->axis[anum]);
    }
  }
  else {
    result[0] = joint->axis[anum][0];
    result[1] = joint->axis[anum][1];
    result[2] = joint->axis[anum][2];
  }
}


extern "C" int dJointGetAMotorAxisRel (dxJointAMotor *joint, int anum)
{
  dAASSERT(joint && anum >= 0 && anum < 3);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  if (anum < 0) anum = 0;
  if (anum > 2) anum = 2;
  return joint->rel[anum];
}


extern "C" dReal dJointGetAMotorAngle (dxJointAMotor *joint, int anum)
{
  dAASSERT(joint && anum >= 0 && anum < 3);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  if (anum < 0) anum = 0;
  if (anum > 3) anum = 3;
  return joint->angle[anum];
}


extern "C" dReal dJointGetAMotorAngleRate (dxJointAMotor *joint, int anum)
{
  // @@@
  dDebug (0,"not yet implemented");
  return 0;
}


extern "C" dReal dJointGetAMotorParam (dxJointAMotor *joint, int parameter)
{
  dAASSERT(joint);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  int anum = parameter >> 8;
  if (anum < 0) anum = 0;
  if (anum > 2) anum = 2;
  parameter &= 0xff;
  return joint->limot[anum].get (parameter);
}


extern "C" int dJointGetAMotorMode (dxJointAMotor *joint)
{
  dAASSERT(joint);
  dUASSERT(joint->vtable == &__damotor_vtable,"joint is not an amotor");
  return joint->mode;
}


dxJoint::Vtable __damotor_vtable = {
  sizeof(dxJointAMotor),
  (dxJoint::init_fn*) amotorInit,
  (dxJoint::getInfo1_fn*) amotorGetInfo1,
  (dxJoint::getInfo2_fn*) amotorGetInfo2,
  dJointTypeAMotor};

//****************************************************************************
// fixed joint

static void fixedInit (dxJointFixed *j)
{
  dSetZero (j->offset,4);
}


static void fixedGetInfo1 (dxJointFixed *j, dxJoint::Info1 *info)
{
  info->m = 6;
  info->nub = 6;
}


static void fixedGetInfo2 (dxJointFixed *joint, dxJoint::Info2 *info)
{
  int s = info->rowskip;

  // set jacobian
  info->J1l[0] = 1;
  info->J1l[s+1] = 1;
  info->J1l[2*s+2] = 1;
  info->J1a[3*s] = 1;
  info->J1a[4*s+1] = 1;
  info->J1a[5*s+2] = 1;

  dVector3 ofs;
  if (joint->node[1].body) {
    dMULTIPLY0_331 (ofs,joint->node[0].body->R,joint->offset);
    dCROSSMAT (info->J1a,ofs,s,+,-);
    info->J2l[0] = -1;
    info->J2l[s+1] = -1;
    info->J2l[2*s+2] = -1;
    info->J2a[3*s] = -1;
    info->J2a[4*s+1] = -1;
    info->J2a[5*s+2] = -1;
  }

  // set right hand side for the first three rows (linear)
  dReal k = info->fps * info->erp;
  if (joint->node[1].body) {
    for (int j=0; j<3; j++)
      info->c[j] = k * (joint->node[1].body->pos[j] -
			joint->node[0].body->pos[j] + ofs[j]);
  }
  else {
    for (int j=0; j<3; j++)
      info->c[j] = k * (joint->offset[j] - joint->node[0].body->pos[j]);
  }

  // set right hand side for the next three rows (angular). this code is
  // borrowed from the slider, so look at the comments there.
  // @@@ make a function common to both the slider and this joint !!!

  // get qerr = relative rotation (rotation error) between two bodies
  dQuaternion qerr,e;
  if (joint->node[1].body) {
    dQMultiply1 (qerr,joint->node[0].body->q,joint->node[1].body->q);
  }
  else {
    qerr[0] = joint->node[0].body->q[0];
    for (int i=1; i<4; i++) qerr[i] = -joint->node[0].body->q[i];
  }
  if (qerr[0] < 0) {
    qerr[1] = -qerr[1];		// adjust sign of qerr to make theta small
    qerr[2] = -qerr[2];
    qerr[3] = -qerr[3];
  }
  dMULTIPLY0_331 (e,joint->node[0].body->R,qerr+1); // @@@ bad SIMD padding!
  info->c[3] = 2*k * e[0];
  info->c[4] = 2*k * e[1];
  info->c[5] = 2*k * e[2];
}


extern "C" void dJointSetFixed (dxJointFixed *joint)
{
  dUASSERT(joint,"bad joint argument");
  dUASSERT(joint->vtable == &__dfixed_vtable,"joint is not fixed");
  int i;

  // compute the offset between the bodies
  if (joint->node[0].body) {
    if (joint->node[1].body) {
      dReal ofs[4];
      for (i=0; i<4; i++) ofs[i] = joint->node[0].body->pos[i];
      for (i=0; i<4; i++) ofs[i] -= joint->node[1].body->pos[i];
      dMULTIPLY1_331 (joint->offset,joint->node[0].body->R,ofs);
    }
    else {
      for (i=0; i<4; i++) joint->offset[i] = joint->node[0].body->pos[i];
    }
  }
}


dxJoint::Vtable __dfixed_vtable = {
  sizeof(dxJointFixed),
  (dxJoint::init_fn*) fixedInit,
  (dxJoint::getInfo1_fn*) fixedGetInfo1,
  (dxJoint::getInfo2_fn*) fixedGetInfo2,
  dJointTypeFixed};

//****************************************************************************
// null joint

static void nullGetInfo1 (dxJointNull *j, dxJoint::Info1 *info)
{
  info->m = 0;
  info->nub = 0;
}


static void nullGetInfo2 (dxJointNull *joint, dxJoint::Info2 *info)
{
  dDebug (0,"this should never get called");
}


dxJoint::Vtable __dnull_vtable = {
  sizeof(dxJointNull),
  (dxJoint::init_fn*) 0,
  (dxJoint::getInfo1_fn*) nullGetInfo1,
  (dxJoint::getInfo2_fn*) nullGetInfo2,
  dJointTypeNull};
