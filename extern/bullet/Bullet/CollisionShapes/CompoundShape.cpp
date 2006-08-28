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

#include "CompoundShape.h"


#include "CollisionShape.h"


CompoundShape::CompoundShape()
:m_localAabbMin(1e30f,1e30f,1e30f),
m_localAabbMax(-1e30f,-1e30f,-1e30f),
m_aabbTree(0),
m_collisionMargin(0.f),
m_localScaling(1.f,1.f,1.f)
{
}


CompoundShape::~CompoundShape()
{
}

void	CompoundShape::AddChildShape(const SimdTransform& localTransform,CollisionShape* shape)
{
	m_childTransforms.push_back(localTransform);
	m_childShapes.push_back(shape);

	//extend the local aabbMin/aabbMax
	SimdVector3 localAabbMin,localAabbMax;
	shape->GetAabb(localTransform,localAabbMin,localAabbMax);
	for (int i=0;i<3;i++)
	{
		if (m_localAabbMin[i] > localAabbMin[i])
		{
			m_localAabbMin[i] = localAabbMin[i];
		}
		if (m_localAabbMax[i] < localAabbMax[i])
		{
			m_localAabbMax[i] = localAabbMax[i];
		}

	}
}



	///GetAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
void CompoundShape::GetAabb(const SimdTransform& trans,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	SimdVector3 localHalfExtents = 0.5f*(m_localAabbMax-m_localAabbMin);
	SimdVector3 localCenter = 0.5f*(m_localAabbMax+m_localAabbMin);
	
	SimdMatrix3x3 abs_b = trans.getBasis().absolute();  

	SimdPoint3 center = trans(localCenter);

	SimdVector3 extent = SimdVector3(abs_b[0].dot(localHalfExtents),
		   abs_b[1].dot(localHalfExtents),
		  abs_b[2].dot(localHalfExtents));
	extent += SimdVector3(GetMargin(),GetMargin(),GetMargin());

	aabbMin = center - extent;
	aabbMax = center + extent;
}

void	CompoundShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//approximation: take the inertia from the aabb for now
	SimdTransform ident;
	ident.setIdentity();
	SimdVector3 aabbMin,aabbMax;
	GetAabb(ident,aabbMin,aabbMax);
	
	SimdVector3 halfExtents = (aabbMax-aabbMin)*0.5f;
	
	SimdScalar lx=2.f*(halfExtents.x());
	SimdScalar ly=2.f*(halfExtents.y());
	SimdScalar lz=2.f*(halfExtents.z());

	inertia[0] = mass/(12.0f) * (ly*ly + lz*lz);
	inertia[1] = mass/(12.0f) * (lx*lx + lz*lz);
	inertia[2] = mass/(12.0f) * (lx*lx + ly*ly);

}

	
	
