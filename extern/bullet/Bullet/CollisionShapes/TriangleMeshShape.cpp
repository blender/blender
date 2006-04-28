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
: m_meshInterface(meshInterface),
m_collisionMargin(CONVEX_DISTANCE_MARGIN)
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

void	TriangleMeshShape::RecalcLocalAabb()
{
	for (int i=0;i<3;i++)
	{
		SimdVector3 vec(0.f,0.f,0.f);
		vec[i] = 1.f;
		SimdVector3 tmp = LocalGetSupportingVertex(vec);
		m_localAabbMax[i] = tmp[i]+m_collisionMargin;
		vec[i] = -1.f;
		tmp = LocalGetSupportingVertex(vec);
		m_localAabbMin[i] = tmp[i]-m_collisionMargin;
	}
}



class SupportVertexCallback : public TriangleCallback
{

	SimdVector3 m_supportVertexLocal;
public:

	SimdTransform	m_worldTrans;
	SimdScalar m_maxDot;
	SimdVector3 m_supportVecLocal;

	SupportVertexCallback(const SimdVector3& supportVecWorld,const SimdTransform& trans)
		: m_supportVertexLocal(0.f,0.f,0.f), m_worldTrans(trans) ,m_maxDot(-1e30f)
		
	{
		m_supportVecLocal = supportVecWorld * m_worldTrans.getBasis();
	}

	virtual void ProcessTriangle( SimdVector3* triangle,int partId, int triangleIndex)
	{
		for (int i=0;i<3;i++)
		{
			SimdScalar dot = m_supportVecLocal.dot(triangle[i]);
			if (dot > m_maxDot)
			{
				m_maxDot = dot;
				m_supportVertexLocal = triangle[i];
			}
		}
	}

	SimdVector3 GetSupportVertexWorldSpace()
	{
		return m_worldTrans(m_supportVertexLocal);
	}

	SimdVector3	GetSupportVertexLocal()
	{
		return m_supportVertexLocal;
	}

};

	
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


void	TriangleMeshShape::ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
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





void	TriangleMeshShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//moving concave objects not supported
	assert(0);
	inertia.setValue(0.f,0.f,0.f);
}


SimdVector3 TriangleMeshShape::LocalGetSupportingVertex(const SimdVector3& vec) const
{
	SimdVector3 supportVertex;

	SimdTransform ident;
	ident.setIdentity();

	SupportVertexCallback supportCallback(vec,ident);

	SimdVector3 aabbMax(1e30f,1e30f,1e30f);
	
	ProcessAllTriangles(&supportCallback,-aabbMax,aabbMax);
		
	supportVertex = supportCallback.GetSupportVertexLocal();

	return supportVertex;
}
