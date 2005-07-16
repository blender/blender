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
#ifndef BU_SHAPE
#define BU_SHAPE

#include <SimdPoint3.h>
#include <SimdMatrix3x3.h>
#include <CollisionShapes/ConvexShape.h>


///PolyhedralConvexShape is an interface class for feature based (vertex/edge/face) convex shapes.
class PolyhedralConvexShape : public ConvexShape
{

public:

	PolyhedralConvexShape();
	
	//brute force implementations
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;


	virtual int	GetNumVertices() const = 0 ;
	virtual int GetNumEdges() const = 0;
	virtual void GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const = 0;
	virtual void GetVertex(int i,SimdPoint3& vtx) const = 0;
	virtual int	GetNumPlanes() const = 0;
	virtual void GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i ) const = 0;
//	virtual int GetIndex(int i) const = 0 ; 

	virtual	bool IsInside(const SimdPoint3& pt,SimdScalar tolerance) const = 0;
	
};

#endif //BU_SHAPE
