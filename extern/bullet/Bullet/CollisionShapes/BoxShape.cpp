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

#include "BoxShape.h"

SimdVector3 BoxShape::GetHalfExtents() const
{ 
	SimdVector3 scalingCorrectedHalfExtents = m_boxHalfExtents1 * m_localScaling;
	SimdVector3 marginCorrection (CONVEX_DISTANCE_MARGIN,CONVEX_DISTANCE_MARGIN,CONVEX_DISTANCE_MARGIN);
	return (scalingCorrectedHalfExtents - marginCorrection).absolute();
}



void BoxShape::GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	SimdVector3 halfExtents = GetHalfExtents();

	SimdMatrix3x3 abs_b = t.getBasis().absolute();  
	SimdPoint3 center = t.getOrigin();
	SimdVector3 extent = SimdVector3(abs_b[0].dot(halfExtents),
		   abs_b[1].dot(halfExtents),
		  abs_b[2].dot(halfExtents));
	extent += SimdVector3(GetMargin(),GetMargin(),GetMargin());


	//todo: this is a quick fix, we need to enlarge the aabb dependent on several criteria
//	SimdVector3 extra(1,1,1);
//	extent += extra;
	
	aabbMin = center - extent;
	aabbMax = center + extent;


}
