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

#ifndef _ODE_COMMON_H_
#define _ODE_COMMON_H_

#include <ode/config.h>
#include <ode/error.h>

#ifdef __cplusplus
extern "C" {
#endif


/* configuration stuff */

/* the efficient alignment. most platforms align data structures to some
 * number of bytes, but this is not always the most efficient alignment.
 * for example, many x86 compilers align to 4 bytes, but on a pentium it
 * is important to align doubles to 8 byte boundaries (for speed), and
 * the 4 floats in a SIMD register to 16 byte boundaries. many other
 * platforms have similar behavior. setting a larger alignment can waste
 * a (very) small amount of memory. NOTE: this number must be a power of
 * two. this is set to 16 by default.
 */
#define EFFICIENT_ALIGNMENT 16


/* constants */

/* pi and 1/sqrt(2) are defined here if necessary because they don't get
 * defined in <math.h> on some platforms (like MS-Windows)
 */

#ifndef M_PI
#define M_PI REAL(3.1415926535897932384626433832795029)
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 REAL(0.7071067811865475244008443621048490)
#endif


/* debugging:
 *   IASSERT  is an internal assertion, i.e. a consistency check. if it fails
 *            we want to know where.
 *   UASSERT  is a user assertion, i.e. if it fails a nice error message
 *            should be printed for the user.
 *   AASSERT  is an arguments assertion, i.e. if it fails "bad argument(s)"
 *            is printed.
 *   DEBUGMSG just prints out a message
 */

#ifndef dNODEBUG
#ifdef __GNUC__
#define dIASSERT(a) if (!(a)) dDebug (d_ERR_IASSERT, \
  "assertion \"" #a "\" failed in %s() [%s]",__FUNCTION__,__FILE__);
#define dUASSERT(a,msg) if (!(a)) dDebug (d_ERR_UASSERT, \
  msg " in %s()", __FUNCTION__);
#define dDEBUGMSG(msg) dMessage (d_ERR_UASSERT, \
  msg " in %s()", __FUNCTION__);
#else
#define dIASSERT(a) if (!(a)) dDebug (d_ERR_IASSERT, \
  "assertion \"" #a "\" failed in %s:%d",__FILE__,__LINE__);
#define dUASSERT(a,msg) if (!(a)) dDebug (d_ERR_UASSERT, \
  msg " (%s:%d)", __FILE__,__LINE__);
#define dDEBUGMSG(msg) dMessage (d_ERR_UASSERT, \
  msg " (%s:%d)", __FILE__,__LINE__);
#endif
#else
#define dIASSERT(a) ;
#define dUASSERT(a,msg) ;
#define dDEBUGMSG(msg) ;
#endif
#define dAASSERT(a) dUASSERT(a,"Bad argument(s)")

/* floating point data type, vector, matrix and quaternion types */

#if defined(dSINGLE)
typedef float dReal;
#elif defined(dDOUBLE)
typedef double dReal;
#else
#error You must #define dSINGLE or dDOUBLE
#endif


/* round an integer up to a multiple of 4, except that 0 and 1 are unmodified
 * (used to compute matrix leading dimensions)
 */
#define dPAD(a) (((a) > 1) ? ((((a)-1)|3)+1) : (a))

/* these types are mainly just used in headers */
typedef dReal dVector3[4];
typedef dReal dVector4[4];
typedef dReal dMatrix3[4*3];
typedef dReal dMatrix4[4*4];
typedef dReal dMatrix6[8*6];
typedef dReal dQuaternion[4];


/* precision dependent scalar math functions */

#if defined(dSINGLE)

#define REAL(x) (x ## f)			/* form a constant */
#define dRecip(x) ((float)(1.0f/(x)))		/* reciprocal */
#define dSqrt(x) ((float)sqrt(x))		/* square root */
#define dRecipSqrt(x) ((float)(1.0f/sqrt(x)))	/* reciprocal square root */
#define dSin(x) ((float)sin(x))			/* sine */
#define dCos(x) ((float)cos(x))			/* cosine */
#define dFabs(x) ((float)fabs(x))		/* absolute value */
#define dAtan2(y,x) ((float)atan2((y),(x)))	/* arc tangent with 2 args */

#elif defined(dDOUBLE)

#define REAL(x) (x)
#define dRecip(x) (1.0/(x))
#define dSqrt(x) sqrt(x)
#define dRecipSqrt(x) (1.0/sqrt(x))
#define dSin(x) sin(x)
#define dCos(x) cos(x)
#define dFabs(x) fabs(x)
#define dAtan2(y,x) atan2((y),(x))

#else
#error You must #define dSINGLE or dDOUBLE
#endif


/* utility */


/* round something up to be a multiple of the EFFICIENT_ALIGNMENT */

#define dEFFICIENT_SIZE(x) ((((x)-1)|(EFFICIENT_ALIGNMENT-1))+1)


/* alloca aligned to the EFFICIENT_ALIGNMENT. note that this can waste
 * up to 15 bytes per allocation, depending on what alloca() returns.
 */

#define dALLOCA16(n) \
  ((char*)dEFFICIENT_SIZE(((int)(alloca((n)+(EFFICIENT_ALIGNMENT-1))))))


/* internal object types (all prefixed with `dx') */

struct dxWorld;		/* dynamics world */
struct dxSpace;		/* collision space */
struct dxBody;		/* rigid body (dynamics object) */
struct dxGeom;		/* geometry (collision object) */
struct dxJoint;
struct dxJointNode;
struct dxJointGroup;

typedef struct dxWorld *dWorldID;
typedef struct dxSpace *dSpaceID;
typedef struct dxBody *dBodyID;
typedef struct dxGeom *dGeomID;
typedef struct dxJoint *dJointID;
typedef struct dxJointGroup *dJointGroupID;


/* error numbers */

enum {
  d_ERR_UNKNOWN = 0,		/* unknown error */
  d_ERR_IASSERT,		/* internal assertion failed */
  d_ERR_UASSERT,		/* user assertion failed */
  d_ERR_LCP			/* user assertion failed */
};


/* joint type numbers */

enum {
  dJointTypeNone = 0,		/* or "unknown" */
  dJointTypeBall,
  dJointTypeHinge,
  dJointTypeSlider,
  dJointTypeContact,
  dJointTypeUniversal,
  dJointTypeHinge2,
  dJointTypeFixed,
  dJointTypeNull,
  dJointTypeAMotor
};


/* an alternative way of setting joint parameters, using joint parameter
 * structures and member constants. we don't actually do this yet.
 */

/*
typedef struct dLimot {
  int mode;
  dReal lostop, histop;
  dReal vel, fmax;
  dReal fudge_factor;
  dReal bounce, soft;
  dReal suspension_erp, suspension_cfm;
} dLimot;

enum {
  dLimotLoStop		= 0x0001,
  dLimotHiStop		= 0x0002,
  dLimotVel		= 0x0004,
  dLimotFMax		= 0x0008,
  dLimotFudgeFactor	= 0x0010,
  dLimotBounce		= 0x0020,
  dLimotSoft		= 0x0040
};
*/


/* standard joint parameter names. why are these here? - because we don't want
 * to include all the joint function definitions in joint.cpp. hmmmm.
 * MSVC complains if we call D_ALL_PARAM_NAMES_X with a blank second argument,
 * which is why we have the D_ALL_PARAM_NAMES macro as well. please copy and
 * paste between these two.
 */

#define D_ALL_PARAM_NAMES(start) \
  /* parameters for limits and motors */ \
  dParamLoStop = start, \
  dParamHiStop, \
  dParamVel, \
  dParamFMax, \
  dParamFudgeFactor, \
  dParamBounce, \
  dParamCFM, \
  dParamStopERP, \
  dParamStopCFM, \
  /* parameters for suspension */ \
  dParamSuspensionERP, \
  dParamSuspensionCFM,

#define D_ALL_PARAM_NAMES_X(start,x) \
  /* parameters for limits and motors */ \
  dParamLoStop ## x = start, \
  dParamHiStop ## x, \
  dParamVel ## x, \
  dParamFMax ## x, \
  dParamFudgeFactor ## x, \
  dParamBounce ## x, \
  dParamCFM ## x, \
  dParamStopERP ## x, \
  dParamStopCFM ## x, \
  /* parameters for suspension */ \
  dParamSuspensionERP ## x, \
  dParamSuspensionCFM ## x,

enum {
  D_ALL_PARAM_NAMES(0)
  D_ALL_PARAM_NAMES_X(0x100,2)
  D_ALL_PARAM_NAMES_X(0x200,3)

  /* add a multiple of this constant to the basic parameter numbers to get
   * the parameters for the second, third etc axes.
   */
  dParamGroup=0x100
};


/* angular motor mode numbers */

enum{
  dAMotorUser = 0,
  dAMotorEuler = 1
};


/* joint force feedback information */

typedef struct dJointFeedback {
  dVector3 f1;		// force applied to body 1
  dVector3 t1;		// torque applied to body 1
  dVector3 f2;		// force applied to body 2
  dVector3 t2;		// torque applied to body 2
} dJointFeedback;


#ifdef __cplusplus
}
#endif

#endif
