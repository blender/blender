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

#include "SphereShape.h"
#include "NarrowPhaseCollision/CollisionMargin.h"

#include "SimdQuaternion.h"


SphereShape ::SphereShape (SimdScalar radius)
: m_radius(radius)
{
	SetMargin( radius );
}

SimdVector3	SphereShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
{
	return SimdVector3(0.f,0.f,0.f);
}

SimdVector3	SphereShape::LocalGetSupportingVertex(const SimdVector3& vec)const
{
	SimdScalar len = vec.length2();
	if (fabsf(len) < 0.0001f)
	{
		return SimdVector3(GetMargin(),GetMargin(),GetMargin());
	} 
	return vec *  (GetMargin() / sqrtf(len));
}


void SphereShape::GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	const SimdVector3& center = t.getOrigin();
	SimdScalar radius = m_radius + CONVEX_DISTANCE_MARGIN;
	radius += 1;

	const SimdVector3 extent(radius,radius,radius);

	aabbMin = center - extent;
	aabbMax = center + extent;
}



void	SphereShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
  SimdScalar elem = 0.4f * mass * m_radius*m_radius;
	inertia[0] = inertia[1] = inertia[2] = elem;
}