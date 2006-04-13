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

#ifndef OBB_TRIANGLE_MINKOWSKI_H
#define OBB_TRIANGLE_MINKOWSKI_H

#include "ConvexShape.h"
#include "CollisionShapes/BoxShape.h"

class TriangleShape : public PolyhedralConvexShape
{


public:

	SimdVector3	m_vertices1[3];


	virtual int GetNumVertices() const
	{
		return 3;
	}

	const SimdVector3& GetVertexPtr(int index) const
	{
		return m_vertices1[index];
	}
	virtual void GetVertex(int index,SimdVector3& vert) const
	{
		vert = m_vertices1[index];
	}
	virtual int	GetShapeType() const
	{
		return TRIANGLE_SHAPE_PROXYTYPE;
	}

	virtual int GetNumEdges() const
	{
		return 3;
	}
	
	virtual void GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const
	{
		GetVertex(i,pa);
		GetVertex((i+1)%3,pb);
	}

	virtual void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax)const 
	{
//		ASSERT(0);
		GetAabbSlow(t,aabbMin,aabbMax);
	}

	SimdVector3 LocalGetSupportingVertexWithoutMargin(const SimdVector3& dir)const 
	{
		SimdVector3 dots(dir.dot(m_vertices1[0]), dir.dot(m_vertices1[1]), dir.dot(m_vertices1[2]));
	  	return m_vertices1[dots.maxAxis()];

	}

	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
	{
		for (int i=0;i<numVectors;i++)
		{
			const SimdVector3& dir = vectors[i];
			SimdVector3 dots(dir.dot(m_vertices1[0]), dir.dot(m_vertices1[1]), dir.dot(m_vertices1[2]));
  			supportVerticesOut[i] = m_vertices1[dots.maxAxis()];
		}

	}



	TriangleShape(const SimdVector3& p0,const SimdVector3& p1,const SimdVector3& p2)
	{
		m_vertices1[0] = p0;
		m_vertices1[1] = p1;
		m_vertices1[2] = p2;
	}

	

	virtual void GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i) const
	{
		GetPlaneEquation(i,planeNormal,planeSupport);
	}

	virtual int	GetNumPlanes() const
	{
		return 1;
	}

	void CalcNormal(SimdVector3& normal) const
	{
		normal = (m_vertices1[1]-m_vertices1[0]).cross(m_vertices1[2]-m_vertices1[0]);
		normal.normalize();
	}

	virtual void GetPlaneEquation(int i, SimdVector3& planeNormal,SimdPoint3& planeSupport) const
	{
		CalcNormal(planeNormal);
		planeSupport = m_vertices1[0];
	}

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
	{
		ASSERT(0);
		inertia.setValue(0.f,0.f,0.f);
	}

		virtual	bool IsInside(const SimdPoint3& pt,SimdScalar tolerance) const
	{
		SimdVector3 normal;
		CalcNormal(normal);
		//distance to plane
		SimdScalar dist = pt.dot(normal);
		SimdScalar planeconst = m_vertices1[0].dot(normal);
		dist -= planeconst;
		if (dist >= -tolerance && dist <= tolerance)
		{
			//inside check on edge-planes
			int i;
			for (i=0;i<3;i++)
			{
				SimdPoint3 pa,pb;
				GetEdge(i,pa,pb);
				SimdVector3 edge = pb-pa;
				SimdVector3 edgeNormal = edge.cross(normal);
				edgeNormal.normalize();
				SimdScalar dist = pt.dot( edgeNormal);
				SimdScalar edgeConst = pa.dot(edgeNormal);
				dist -= edgeConst;
				if (dist < -tolerance)
					return false;
			}
			
			return true;
		}

		return false;
	}
		//debugging
		virtual char*	GetName()const
		{
			return "Triangle";
		}


};

#endif //OBB_TRIANGLE_MINKOWSKI_H

