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
}

SimdVector3	SphereShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
{
	float radius = m_radius - GetMargin();

	SimdScalar len = vec.length2();
	if (fabsf(len) < 0.0001f)
	{
		return SimdVector3(m_localScaling[0] * radius,m_localScaling[1]*radius,m_localScaling[2]*radius);
	} 
	return vec *  (m_localScaling*(radius / sqrtf(len)));
}

SimdVector3	SphereShape::LocalGetSupportingVertex(const SimdVector3& vec)const
{
	SimdVector3 supVertex;
	supVertex = LocalGetSupportingVertexWithoutMargin(vec);

	if ( GetMargin()!=0.f )
	{
		SimdVector3 vecnorm = vec;
		if (vecnorm .length2() == 0.f)
		{
			vecnorm.setValue(-1.f,-1.f,-1.f);
		} 
		vecnorm.normalize();
		supVertex+= GetMargin() * vecnorm;
	}
	return supVertex;
}

/*
//broken due to scaling
void SphereShape::GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	const SimdVector3& center = t.getOrigin();
	SimdScalar radius = m_radius;
	
	SimdVector3 extent = m_localScaling*radius;
	extent+= SimdVector3(GetMargin(),GetMargin(),GetMargin());

	aabbMin = center - extent;
	aabbMax = center + extent;
}
*/


void	SphereShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	SimdScalar elem = 0.4f * mass * m_radius*m_radius;
	inertia[0] = inertia[1] = inertia[2] = elem;

}