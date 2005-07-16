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