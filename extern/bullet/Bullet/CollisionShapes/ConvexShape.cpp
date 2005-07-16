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

#include "ConvexShape.h"

ConvexShape::~ConvexShape()
{

}

ConvexShape::ConvexShape()
:m_collisionMargin(CONVEX_DISTANCE_MARGIN),
m_localScaling(1.f,1.f,1.f)
{
}


void	ConvexShape::setLocalScaling(const SimdVector3& scaling)
{
	m_localScaling = scaling;
}



void	ConvexShape::GetAabbSlow(const SimdTransform& trans,SimdVector3&minAabb,SimdVector3&maxAabb) const
{

	SimdScalar margin = 0.05f;
	for (int i=0;i<3;i++)
	{
		SimdVector3 vec(0.f,0.f,0.f);
		vec[i] = 1.f;
		SimdVector3 tmp = trans(LocalGetSupportingVertex(vec*trans.getBasis()));
		maxAabb[i] = tmp[i]+margin;
		vec[i] = -1.f;
		tmp = trans(LocalGetSupportingVertex(vec*trans.getBasis()));
		minAabb[i] = tmp[i]-margin;
	}
};

SimdVector3	ConvexShape::LocalGetSupportingVertex(const SimdVector3& vec)const
 {
	 SimdVector3	supVertex = LocalGetSupportingVertexWithoutMargin(vec);

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


