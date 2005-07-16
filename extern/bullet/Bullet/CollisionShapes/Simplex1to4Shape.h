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
#ifndef BU_SIMPLEX_1TO4_SHAPE
#define BU_SIMPLEX_1TO4_SHAPE


#include <CollisionShapes/PolyhedralConvexShape.h>
#include "BroadphaseCollision/BroadphaseProxy.h"


///BU_Simplex1to4 implements feature based and implicit simplex of up to 4 vertices (tetrahedron, triangle, line, vertex).
class BU_Simplex1to4 : public PolyhedralConvexShape
{
protected:

	int	m_numVertices;
	SimdPoint3	m_vertices[4];

public:
	BU_Simplex1to4();

	BU_Simplex1to4(const SimdPoint3& pt0);
	BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1);
	BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1,const SimdPoint3& pt2);
	BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1,const SimdPoint3& pt2,const SimdPoint3& pt3);

    
	void	Reset()
	{
		m_numVertices = 0;
	}
	
	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
	{
		inertia = SimdVector3(1.f,1.f,1.f);
	}

	virtual int	GetShapeType() const{ return TETRAHEDRAL_SHAPE_PROXYTYPE; }

	void AddVertex(const SimdPoint3& pt);

	//PolyhedralConvexShape interface

	virtual int	GetNumVertices() const;

	virtual int GetNumEdges() const;

	virtual void GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const;
	
	virtual void GetVertex(int i,SimdPoint3& vtx) const;

	virtual int	GetNumPlanes() const;

	virtual void GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i) const;

	virtual int GetIndex(int i) const;

	virtual	bool IsInside(const SimdPoint3& pt,SimdScalar tolerance) const;


	///GetName is for debugging
	virtual  char*	GetName()const { return "BU_Simplex1to4";}

};

#endif //BU_SIMPLEX_1TO4_SHAPE
