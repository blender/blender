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
#include <CollisionShapes/PolyhedralConvexShape.h>

PolyhedralConvexShape::PolyhedralConvexShape()

{
}

SimdVector3	PolyhedralConvexShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec0)const
{
	int i;
	SimdVector3 supVec(0,0,0);

	SimdScalar maxDot(-1e30f);

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

	SimdVector3 vtx;
	SimdScalar newDot;

	for (i=0;i<GetNumVertices();i++)
	{
		GetVertex(i,vtx);
		newDot = vec.dot(vtx);
		if (newDot > maxDot)
		{
			maxDot = newDot;
			supVec = vtx;
		}
	}

	return supVec;

}
