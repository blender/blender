/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#ifndef SIMD_TRANSFORM_UTIL_H
#define SIMD_TRANSFORM_UTIL_H

#include "SimdTransform.h"
#define ANGULAR_MOTION_TRESHOLD 0.5f*SIMD_HALF_PI

/// Utils related to temporal transforms
class SimdTransformUtil
{

public:

	static void IntegrateTransform(const SimdTransform& curTrans,const SimdVector3& linvel,const SimdVector3& angvel,SimdScalar timeStep,SimdTransform& predictedTransform)
	{
		predictedTransform.setOrigin(curTrans.getOrigin() + linvel * timeStep);
//	#define QUATERNION_DERIVATIVE
	#ifdef QUATERNION_DERIVATIVE
		SimdQuaternion orn = curTrans.getRotation();
		orn += (angvel * orn) * (timeStep * 0.5f);
		orn.normalize();
	#else
		//exponential map
		SimdVector3 axis;
		SimdScalar	fAngle = angvel.length(); 
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
			axis   = angvel*( sinf(0.5f*fAngle*timeStep)/fAngle );
		}
		SimdQuaternion dorn (axis.x(),axis.y(),axis.z(),cosf( fAngle*timeStep*0.5f ));
		SimdQuaternion orn0 = curTrans.getRotation();

		SimdQuaternion predictedOrn = dorn * orn0;
	#endif
		predictedTransform.setRotation(predictedOrn);
	}

	static void	CalculateVelocity(const SimdTransform& transform0,const SimdTransform& transform1,SimdScalar timeStep,SimdVector3& linVel,SimdVector3& angVel)
	{
		linVel = (transform1.getOrigin() - transform0.getOrigin()) / timeStep;
#ifdef USE_QUATERNION_DIFF
		SimdQuaternion orn0 = transform0.getRotation();
		SimdQuaternion orn1a = transform1.getRotation();
		SimdQuaternion orn1 = orn0.farthest(orn1a);
		SimdQuaternion dorn = orn1 * orn0.inverse();
#else
		SimdMatrix3x3 dmat = transform1.getBasis() * transform0.getBasis().inverse();
		SimdQuaternion dorn;
		dmat.getRotation(dorn);
#endif//USE_QUATERNION_DIFF

		SimdVector3 axis;
		SimdScalar  angle;
		angle = dorn.getAngle();
		axis = SimdVector3(dorn.x(),dorn.y(),dorn.z());
		axis[3] = 0.f;
		//check for axis length
		SimdScalar len = axis.length2();
		if (len < 0.001f)
			axis = SimdVector3(1.f,0.f,0.f);
		else
			axis /= sqrtf(len);

		
		angVel = axis * angle / timeStep;

	}


};

#endif //SIMD_TRANSFORM_UTIL_H