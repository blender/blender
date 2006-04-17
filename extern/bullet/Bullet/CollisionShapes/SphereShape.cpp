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

void	SphereShape::BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
{
	for (int i=0;i<numVectors;i++)
	{
		supportVerticesOut[i].setValue(0.f,0.f,0.f);
	}
}


SimdVector3	SphereShape::LocalGetSupportingVertex(const SimdVector3& vec)const
{
	SimdVector3 supVertex;
	supVertex = LocalGetSupportingVertexWithoutMargin(vec);

	SimdVector3 vecnorm = vec;
	if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
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

