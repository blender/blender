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

#ifndef CONVEX_HULL_SHAPE_H
#define CONVEX_HULL_SHAPE_H

#include "PolyhedralConvexShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

#include <vector>

///ConvexHullShape implements an implicit (getSupportingVertex) Convex Hull of a Point Cloud (vertices)
///No connectivity is needed. LocalGetSupportingVertex iterates linearly though all vertices.
///on modern hardware, due to cache coherency this isn't that bad. Complex algorithms tend to trash the cash.
///(memory is much slower then the cpu)
class ConvexHullShape : public PolyhedralConvexShape
{
	std::vector<SimdPoint3>	m_points;

public:
	ConvexHullShape(SimdPoint3* points,int numPoints);

	void AddPoint(const SimdPoint3& point)
	{
		m_points.push_back(point);
	}
	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec)const;
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	virtual int	GetShapeType()const { return CONVEX_HULL_SHAPE_PROXYTYPE; }

	//debugging
	virtual char*	GetName()const {return "Convex";}

	
	virtual int	GetNumVertices() const;
	virtual int GetNumEdges() const;
	virtual void GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const;
	virtual void GetVertex(int i,SimdPoint3& vtx) const;
	virtual int	GetNumPlanes() const;
	virtual void GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i ) const;
	virtual	bool IsInside(const SimdPoint3& pt,SimdScalar tolerance) const;



};


#endif //CONVEX_HULL_SHAPE_H