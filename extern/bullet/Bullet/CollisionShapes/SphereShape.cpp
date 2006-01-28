/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
 */

#include "SphereShape.h"
#include "CollisionShapes/CollisionMargin.h"

#include "SimdQuaternion.h"


SphereShape ::SphereShape (SimdScalar radius)
: m_radius(radius)
{	
}

SimdVector3	SphereShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
{
	return SimdVector3(0.f,0.f,0.f);
}

SimdVector3	SphereShape::LocalGetSupportingVertex(const SimdVector3& vec)const
{
	SimdVector3 supVertex;
	supVertex = LocalGetSupportingVertexWithoutMargin(vec);

	SimdVector3 vecnorm = vec;
	if (SimdFuzzyZero(vecnorm .length2()))
	{
		vecnorm.setValue(-1.f,-1.f,-1.f);
	} 
	vecnorm.normalize();
	supVertex+= GetMargin() * vecnorm;
	return supVertex;
}


//broken due to scaling
void SphereShape::GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	const SimdVector3& center = t.getOrigin();
	SimdVector3 extent(GetMargin(),GetMargin(),GetMargin());
	aabbMin = center - extent;
	aabbMax = center + extent;
}



void	SphereShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	SimdScalar elem = 0.4f * mass * GetMargin()*GetMargin();
	inertia[0] = inertia[1] = inertia[2] = elem;

}