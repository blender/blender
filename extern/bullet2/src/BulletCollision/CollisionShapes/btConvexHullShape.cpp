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
#include "btConvexHullShape.h"
#include "BulletCollision/CollisionShapes/btCollisionMargin.h"

#include "LinearMath/btQuaternion.h"



btConvexHullShape ::btConvexHullShape (const float* points,int numPoints,int stride)
{
	m_points.resize(numPoints);

	unsigned char* pointsBaseAddress = (unsigned char*)points;

	for (int i=0;i<numPoints;i++)
	{
		btPoint3* point = (btPoint3*)(pointsBaseAddress + i*stride);
		m_points[i] = point[0];
	}
}

btVector3	btConvexHullShape::localGetSupportingVertexWithoutMargin(const btVector3& vec0)const
{
	btVector3 supVec(0.f,0.f,0.f);
	btScalar newDot,maxDot = -1e30f;

	btVector3 vec = vec0;
	btScalar lenSqr = vec.length2();
	if (lenSqr < 0.0001f)
	{
		vec.setValue(1,0,0);
	} else
	{
		float rlen = 1.f / btSqrt(lenSqr );
		vec *= rlen;
	}


	for (size_t i=0;i<m_points.size();i++)
	{
		btPoint3 vtx = m_points[i] * m_localScaling;

		newDot = vec.dot(vtx);
		if (newDot > maxDot)
		{
			maxDot = newDot;
			supVec = vtx;
		}
	}
	return supVec;
}

void	btConvexHullShape::batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const
{
	btScalar newDot;
	//use 'w' component of supportVerticesOut?
	{
		for (int i=0;i<numVectors;i++)
		{
			supportVerticesOut[i][3] = -1e30f;
		}
	}
	for (size_t i=0;i<m_points.size();i++)
	{
		btPoint3 vtx = m_points[i] * m_localScaling;

		for (int j=0;j<numVectors;j++)
		{
			const btVector3& vec = vectors[j];
			
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
	


btVector3	btConvexHullShape::localGetSupportingVertex(const btVector3& vec)const
{
	btVector3 supVertex = localGetSupportingVertexWithoutMargin(vec);

	if ( getMargin()!=0.f )
	{
		btVector3 vecnorm = vec;
		if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
		{
			vecnorm.setValue(-1.f,-1.f,-1.f);
		} 
		vecnorm.normalize();
		supVertex+= getMargin() * vecnorm;
	}
	return supVertex;
}









//currently just for debugging (drawing), perhaps future support for algebraic continuous collision detection
//Please note that you can debug-draw btConvexHullShape with the Raytracer Demo
int	btConvexHullShape::getNumVertices() const
{
	return m_points.size();
}

int btConvexHullShape::getNumEdges() const
{
	return m_points.size();
}

void btConvexHullShape::getEdge(int i,btPoint3& pa,btPoint3& pb) const
{

	int index0 = i%m_points.size();
	int index1 = (i+1)%m_points.size();
	pa = m_points[index0]*m_localScaling;
	pb = m_points[index1]*m_localScaling;
}

void btConvexHullShape::getVertex(int i,btPoint3& vtx) const
{
	vtx = m_points[i]*m_localScaling;
}

int	btConvexHullShape::getNumPlanes() const
{
	return 0;
}

void btConvexHullShape::getPlane(btVector3& planeNormal,btPoint3& planeSupport,int i ) const
{
	assert(0);
}

//not yet
bool btConvexHullShape::isInside(const btPoint3& pt,btScalar tolerance) const
{
	assert(0);
	return false;
}

