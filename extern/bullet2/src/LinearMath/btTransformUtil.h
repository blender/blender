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

#include "btTransform.h"
#define ANGULAR_MOTION_THRESHOLD btScalar(0.5)*SIMD_HALF_PI



#define SIMDSQRT12 btScalar(0.7071067811865475244008443621048490)

#define btRecipSqrt(x) ((btScalar)(btScalar(1.0)/btSqrt(btScalar(x))))		/* reciprocal square root */

SIMD_FORCE_INLINE btVector3 btAabbSupport(const btVector3& halfExtents,const btVector3& supportDir)
{
	return btVector3(supportDir.x() < btScalar(0.0) ? -halfExtents.x() : halfExtents.x(),
      supportDir.y() < btScalar(0.0) ? -halfExtents.y() : halfExtents.y(),
      supportDir.z() < btScalar(0.0) ? -halfExtents.z() : halfExtents.z()); 
}


SIMD_FORCE_INLINE void btPlaneSpace1 (const btVector3& n, btVector3& p, btVector3& q)
{
  if (btFabs(n.z()) > SIMDSQRT12) {
    // choose p in y-z plane
    btScalar a = n[1]*n[1] + n[2]*n[2];
    btScalar k = btRecipSqrt (a);
    p.setValue(0,-n[2]*k,n[1]*k);
    // set q = n x p
    q.setValue(a*k,-n[0]*p[2],n[0]*p[1]);
  }
  else {
    // choose p in x-y plane
    btScalar a = n.x()*n.x() + n.y()*n.y();
    btScalar k = btRecipSqrt (a);
    p.setValue(-n.y()*k,n.x()*k,0);
    // set q = n x p
    q.setValue(-n.z()*p.y(),n.z()*p.x(),a*k);
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
		btQuaternion predictedOrn = curTrans.getRotation();
		predictedOrn += (angvel * predictedOrn) * (timeStep * btScalar(0.5));
		predictedOrn.normalize();
	#else
		//exponential map
		btVector3 axis;
		btScalar	fAngle = angvel.length(); 
		//limit the angular motion
		if (fAngle*timeStep > ANGULAR_MOTION_THRESHOLD)
		{
			fAngle = ANGULAR_MOTION_THRESHOLD / timeStep;
		}

		if ( fAngle < btScalar(0.001) )
		{
			// use Taylor's expansions of sync function
			axis   = angvel*( btScalar(0.5)*timeStep-(timeStep*timeStep*timeStep)*(btScalar(0.020833333333))*fAngle*fAngle );
		}
		else
		{
			// sync(fAngle) = sin(c*fAngle)/t
			axis   = angvel*( btSin(btScalar(0.5)*fAngle*timeStep)/fAngle );
		}
		btQuaternion dorn (axis.x(),axis.y(),axis.z(),btCos( fAngle*timeStep*btScalar(0.5) ));
		btQuaternion orn0 = curTrans.getRotation();

		btQuaternion predictedOrn = dorn * orn0;
		predictedOrn.normalize();
	#endif
		predictedTransform.setRotation(predictedOrn);
	}

	static void	calculateVelocity(const btTransform& transform0,const btTransform& transform1,btScalar timeStep,btVector3& linVel,btVector3& angVel)
	{
		linVel = (transform1.getOrigin() - transform0.getOrigin()) / timeStep;
		btVector3 axis;
		btScalar  angle;
		calculateDiffAxisAngle(transform0,transform1,axis,angle);
		angVel = axis * angle / timeStep;
	}

	static void calculateDiffAxisAngle(const btTransform& transform0,const btTransform& transform1,btVector3& axis,btScalar& angle)
	{
	
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
	
		///floating point inaccuracy can lead to w component > 1..., which breaks 

		dorn.normalize();
		
		angle = dorn.getAngle();
		axis = btVector3(dorn.x(),dorn.y(),dorn.z());
		axis[3] = btScalar(0.);
		//check for axis length
		btScalar len = axis.length2();
		if (len < SIMD_EPSILON*SIMD_EPSILON)
			axis = btVector3(btScalar(1.),btScalar(0.),btScalar(0.));
		else
			axis /= btSqrt(len);
	}

};

#endif //SIMD_TRANSFORM_UTIL_H

