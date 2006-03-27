/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
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
