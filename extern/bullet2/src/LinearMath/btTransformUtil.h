/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#ifndef SIMD_TRANSFORM_UTIL_H
#define SIMD_TRANSFORM_UTIL_H

#include "LinearMath/btTransform.h"
#define ANGULAR_MOTION_TRESHOLD 0.5f*SIMD_HALF_PI



#define SIMDSQRT12 btScalar(0.7071067811865475244008443621048490)

#define btRecipSqrt(x) ((float)(1.0f/btSqrt(float(x))))		/* reciprocal square root */

inline btVector3 btAabbSupport(const btVector3& halfExtents,const btVector3& supportDir)
{
	return btVector3(supportDir.x() < btScalar(0.0f) ? -halfExtents.x() : halfExtents.x(),
      supportDir.y() < btScalar(0.0f) ? -halfExtents.y() : halfExtents.y(),
      supportDir.z() < btScalar(0.0f) ? -halfExtents.z() : halfExtents.z()); 
}


inline void btPlaneSpace1 (const btVector3& n, btVector3& p, btVector3& q)
{
  if (btFabs(n[2]) > SIMDSQRT12) {
    // choose p in y-z plane
    btScalar a = n[1]*n[1] + n[2]*n[2];
    btScalar k = btRecipSqrt (a);
    p[0] = 0;
    p[1] = -n[2]*k;
    p[2] = n[1]*k;
    // set q = n x p
    q[0] = a*k;
    q[1] = -n[0]*p[2];
    q[2] = n[0]*p[1];
  }
  else {
    // choose p in x-y plane
    btScalar a = n[0]*n[0] + n[1]*n[1];
    btScalar k = btRecipSqrt (a);
    p[0] = -n[1]*k;
    p[1] = n[0]*k;
    p[2] = 0;
    // set q = n x p
    q[0] = -n[2]*p[1];
    q[1] = n[2]*p[0];
    q[2] = a*k;
  }
}



/// Utils related to temporal transforms
class btTransformUtil
{

public:

	static void integrateTransform(const btTransform& curTrans,const btVector3& linvel,const btVector3& angvel,btScalar timeStep,btTransform& predictedTransform)
	{
		predictedTransform.setOrigin(curTrans.getOrigin() + linvel * timeStep);
//	#define QUATERNION_DERIVATIVE
	#ifdef QUATERNION_DERIVATIVE
		btQuaternion orn = curTrans.getRotation();
		orn += (angvel * orn) * (timeStep * 0.5f);
		orn.normalize();
	#else
		//exponential map
		btVector3 axis;
		btScalar	fAngle = angvel.length(); 
		//limit the angular motion
		if (fAngle*timeStep > ANGULAR_MOTION_TRESHOLD)
		{
			fAngle = ANGULAR_MOTION_TRESHOLD / timeStep;
		}

		if ( fAngle < 0.001f )
		{
			// use Taylor's expansions of sync function
			axis   = angvel*( 0.5f*timeStep-(timeStep*timeStep*timeStep)*(0.020833333333f)*fAngle*fAngle );
		}
		else
		{
			// sync(fAngle) = sin(c*fAngle)/t
			axis   = angvel*( btSin(0.5f*fAngle*timeStep)/fAngle );
		}
		btQuaternion dorn (axis.x(),axis.y(),axis.z(),btCos( fAngle*timeStep*0.5f ));
		btQuaternion orn0 = curTrans.getRotation();

		btQuaternion predictedOrn = dorn * orn0;
	#endif
		predictedTransform.setRotation(predictedOrn);
	}

	static void	calculateVelocity(const btTransform& transform0,const btTransform& transform1,btScalar timeStep,btVector3& linVel,btVector3& angVel)
	{
		linVel = (transform1.getOrigin() - transform0.getOrigin()) / timeStep;
#ifdef USE_QUATERNION_DIFF
		btQuaternion orn0 = transform0.getRotation();
		btQuaternion orn1a = transform1.getRotation();
		btQuaternion orn1 = orn0.farthest(orn1a);
		btQuaternion dorn = orn1 * orn0.inverse();
#else
		btMatrix3x3 dmat = transform1.getBasis() * transform0.getBasis().inverse();
		btQuaternion dorn;
		dmat.getRotation(dorn);
#endif//USE_QUATERNION_DIFF

		btVector3 axis;
		btScalar  angle;
		angle = dorn.getAngle();
		axis = btVector3(dorn.x(),dorn.y(),dorn.z());
		axis[3] = 0.f;
		//check for axis length
		btScalar len = axis.length2();
		if (len < SIMD_EPSILON*SIMD_EPSILON)
			axis = btVector3(1.f,0.f,0.f);
		else
			axis /= btSqrt(len);

		
		angVel = axis * angle / timeStep;

	}


};

#endif //SIMD_TRANSFORM_UTIL_H

