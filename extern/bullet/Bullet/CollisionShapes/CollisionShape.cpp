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
#include "CollisionShapes/CollisionShape.h"

void	CollisionShape::GetBoundingSphere(SimdVector3& center,SimdScalar& radius) const
{
	SimdTransform tr;
	tr.setIdentity();
	SimdVector3 aabbMin,aabbMax;

	GetAabb(tr,aabbMin,aabbMax);

	radius = (aabbMax-aabbMin).length()*0.5f;
	center = (aabbMin+aabbMax)*0.5f;
}

float	CollisionShape::GetAngularMotionDisc() const
{
	SimdVector3	center;
	float disc;
	GetBoundingSphere(center,disc);
	disc += (center).length();
	return disc;
}

void CollisionShape::CalculateTemporalAabb(const SimdTransform& curTrans,const SimdVector3& linvel,const SimdVector3& angvel,SimdScalar timeStep, SimdVector3& temporalAabbMin,SimdVector3& temporalAabbMax)
{
	//start with static aabb
	GetAabb(curTrans,temporalAabbMin,temporalAabbMax);

	float temporalAabbMaxx = temporalAabbMax.getX();
	float temporalAabbMaxy = temporalAabbMax.getY();
	float temporalAabbMaxz = temporalAabbMax.getZ();
	float temporalAabbMinx = temporalAabbMin.getX();
	float temporalAabbMiny = temporalAabbMin.getY();
	float temporalAabbMinz = temporalAabbMin.getZ();

	// add linear motion
	SimdVector3 linMotion = linvel*timeStep;
	//todo: simd would have a vector max/min operation, instead of per-element access
	if (linMotion.x() > 0.f)
		temporalAabbMaxx += linMotion.x(); 
	else
		temporalAabbMinx += linMotion.x();
	if (linMotion.y() > 0.f)
		temporalAabbMaxy += linMotion.y(); 
	else
		temporalAabbMiny += linMotion.y();
	if (linMotion.z() > 0.f)
		temporalAabbMaxz += linMotion.z(); 
	else
		temporalAabbMinz += linMotion.z();

	//add conservative angular motion
	SimdScalar angularMotion = angvel.length() * GetAngularMotionDisc() * timeStep;
	SimdVector3 angularMotion3d(angularMotion,angularMotion,angularMotion);
	temporalAabbMin = SimdVector3(temporalAabbMinx,temporalAabbMiny,temporalAabbMinz);
	temporalAabbMax = SimdVector3(temporalAabbMaxx,temporalAabbMaxy,temporalAabbMaxz);

	temporalAabbMin -= angularMotion3d;
	temporalAabbMax += angularMotion3d;
}
