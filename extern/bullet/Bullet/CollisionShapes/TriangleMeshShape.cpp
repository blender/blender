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

#include "TriangleMeshShape.h"
#include "SimdVector3.h"
#include "SimdQuaternion.h"
#include "StridingMeshInterface.h"
#include "AabbUtil2.h"
#include "CollisionShapes/CollisionMargin.h"

#include "stdio.h"

TriangleMeshShape::TriangleMeshShape(StridingMeshInterface* meshInterface)
: m_meshInterface(meshInterface)
{
	RecalcLocalAabb();
}


TriangleMeshShape::~TriangleMeshShape()
{
		
}




void TriangleMeshShape::GetAabb(const SimdTransform& trans,SimdVector3& aabbMin,SimdVector3& aabbMax) const
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



class LocalAabbCalculator : public TriangleCallback
{

public:

	SimdVector3 m_localAabbMin;
	SimdVector3 m_localAabbMax;

	LocalAabbCalculator()
		: m_localAabbMin(1e30f,1e30f,1e30f), m_localAabbMax(-1e30f,-1e30f,-1e30f)
	{
	}

	virtual void ProcessTriangle( SimdVector3* triangle,int partId, int triangleIndex)
	{
		for (int i=0;i<3;i++)
		{
			m_localAabbMin.setMin(triangle[i]);
			m_localAabbMax.setMax(triangle[i]);
		}
	}
};


void	TriangleMeshShape::RecalcLocalAabb()
{

		LocalAabbCalculator aabbCalculator;
		SimdVector3 aabbMax(1e30f,1e30f,1e30f);

		NonVirtualProcessAllTriangles(&aabbCalculator,-aabbMax,aabbMax);
		SimdVector3 marginVec(m_collisionMargin,m_collisionMargin,m_collisionMargin);
		m_localAabbMax = aabbCalculator.m_localAabbMax + marginVec;
		m_localAabbMin = aabbCalculator.m_localAabbMin - marginVec;
		
}

	
void TriangleMeshShape::setLocalScaling(const SimdVector3& scaling)
{
	m_meshInterface->setScaling(scaling);
	RecalcLocalAabb();
}

const SimdVector3& TriangleMeshShape::getLocalScaling() const
{
	return m_meshInterface->getScaling();
}






//#define DEBUG_TRIANGLE_MESH

void	TriangleMeshShape::NonVirtualProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{
	struct FilteredCallback : public InternalTriangleIndexCallback
	{
		TriangleCallback* m_callback;
		SimdVector3 m_aabbMin;
		SimdVector3 m_aabbMax;

		FilteredCallback(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax)
			:m_callback(callback),
			m_aabbMin(aabbMin),
			m_aabbMax(aabbMax)
		{
		}

		virtual void InternalProcessTriangleIndex(SimdVector3* triangle,int partId,int triangleIndex)
		{
			if (TestTriangleAgainstAabb2(&triangle[0],m_aabbMin,m_aabbMax))
			{
				//check aabb in triangle-space, before doing this
				m_callback->ProcessTriangle(triangle,partId,triangleIndex);
			}
			
		}

	};

	FilteredCallback filterCallback(callback,aabbMin,aabbMax);

	m_meshInterface->InternalProcessAllTriangles(&filterCallback,aabbMin,aabbMax);

}


void	TriangleMeshShape::ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{
	NonVirtualProcessAllTriangles(callback,aabbMin,aabbMax);	
}

void	TriangleMeshShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//moving concave objects not supported
	assert(0);
	inertia.setValue(0.f,0.f,0.f);
}

