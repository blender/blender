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

	SimdScalar margin = GetMargin();
	for (int i=0;i<3;i++)
	{
		SimdVector3 vec(0.f,0.f,0.f);
		vec[i] = 1.f;

		SimdVector3 sv = LocalGetSupportingVertex(vec*trans.getBasis());

		SimdVector3 tmp = trans(sv);
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
		if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
		{
			vecnorm.setValue(-1.f,-1.f,-1.f);
		} 
		vecnorm.normalize();
		supVertex+= GetMargin() * vecnorm;
	}
	return supVertex;

 }


