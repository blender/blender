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


	for (size_t i=0;i<m_points.size();i++)
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

void	ConvexHullShape::BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
{
	SimdScalar newDot;
	//use 'w' component of supportVerticesOut?
	{
		for (int i=0;i<numVectors;i++)
		{
			supportVerticesOut[i][3] = -1e30f;
		}
	}
	for (size_t i=0;i<m_points.size();i++)
	{
		SimdPoint3 vtx = m_points[i] * m_localScaling;

		for (int j=0;j<numVectors;j++)
		{
			const SimdVector3& vec = vectors[j];
			
			newDot = vec.dot(vtx);
			if (newDot > supportVerticesOut[j][3])
			{
				//WARNING: don't swap next lines, the w component would get overwritten!
				supportVerticesOut[j] = vtx;
				supportVerticesOut[j][3] = newDot;
			}
		}
	}



}
	


SimdVector3	ConvexHullShape::LocalGetSupportingVertex(const SimdVector3& vec)const
{
	SimdVector3 supVertex = LocalGetSupportingVertexWithoutMargin(vec);

	if ( GetMargin()!=0.f )
	{
		SimdVector3 vecnorm = vec;
		if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
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

