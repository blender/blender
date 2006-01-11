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

#include "ConvexHullShape.h"
#include "CollisionShapes/CollisionMargin.h"

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
		float rlen = 1.f / SimdSqrt(lenSqr );
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
	pa = m_points[index0]*m_localScaling;
	pb = m_points[index1]*m_localScaling;
}

void ConvexHullShape::GetVertex(int i,SimdPoint3& vtx) const
{
	vtx = m_points[i]*m_localScaling;
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

