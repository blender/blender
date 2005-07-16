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

#include "ConvexHullShape.h"
#include "NarrowPhaseCollision/CollisionMargin.h"

#include "SimdQuaternion.h"


ConvexHullShape ::ConvexHullShape (SimdPoint3* points,int numPoints)
{
	m_points.resize(numPoints);
	for (int i=0;i<numPoints;i++)
		m_points[i] = points[i];
}

SimdVector3	ConvexHullShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec0)const
{
	SimdVector3 supVec(0.f,0.f,0.f);
	SimdScalar newDot,maxDot = -1e30f;

	SimdVector3 vec = vec0;
	SimdScalar lenSqr = vec.length2();
	if (lenSqr < 0.0001f)
	{
		vec.setValue(1,0,0);
	} else
	{
		float rlen = 1.f / sqrtf(lenSqr );
		vec *= rlen;
	}


	for (int i=0;i<m_points.size();i++)
	{
		SimdPoint3 vtx = m_points[i] * m_localScaling;

		newDot = vec.dot(vtx);
		if (newDot > maxDot)
		{
			maxDot = newDot;
			supVec = vtx;
		}
	}
	return supVec;
}


SimdVector3	ConvexHullShape::LocalGetSupportingVertex(const SimdVector3& vec)const
{
	SimdVector3 supVertex = LocalGetSupportingVertexWithoutMargin(vec);

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





void	ConvexHullShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//not yet, return box inertia

	float margin = GetMargin();

	SimdTransform ident;
	ident.setIdentity();
	SimdVector3 aabbMin,aabbMax;
	GetAabb(ident,aabbMin,aabbMax);
	SimdVector3 halfExtents = (aabbMax-aabbMin)*0.5f;

	SimdScalar lx=2.f*(halfExtents.x()+margin);
	SimdScalar ly=2.f*(halfExtents.y()+margin);
	SimdScalar lz=2.f*(halfExtents.z()+margin);
	const SimdScalar x2 = lx*lx;
	const SimdScalar y2 = ly*ly;
	const SimdScalar z2 = lz*lz;
	const SimdScalar scaledmass = mass * 0.08333333f;

	inertia = scaledmass * (SimdVector3(y2+z2,x2+z2,x2+y2));

}



//currently just for debugging (drawing), perhaps future support for algebraic continuous collision detection
//Please note that you can debug-draw ConvexHullShape with the Raytracer Demo
int	ConvexHullShape::GetNumVertices() const
{
	return m_points.size();
}

int ConvexHullShape::GetNumEdges() const
{
	return m_points.size()*m_points.size();
}

void ConvexHullShape::GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const
{

	int index0 = i%m_points.size();
	int index1 = i/m_points.size();
	pa = m_points[index0];
	pb = m_points[index1];
}

void ConvexHullShape::GetVertex(int i,SimdPoint3& vtx) const
{
	vtx = m_points[i];
}

int	ConvexHullShape::GetNumPlanes() const
{
	return 0;
}

void ConvexHullShape::GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i ) const
{
	assert(0);
}

//not yet
bool ConvexHullShape::IsInside(const SimdPoint3& pt,SimdScalar tolerance) const
{
	assert(0);
	return false;
}

