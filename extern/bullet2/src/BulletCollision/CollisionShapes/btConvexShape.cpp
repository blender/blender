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

#include "btConvexShape.h"

btConvexShape::btConvexShape()
: m_localScaling(1.f,1.f,1.f),
m_collisionMargin(CONVEX_DISTANCE_MARGIN)
{
}


void	btConvexShape::setLocalScaling(const btVector3& scaling)
{
	m_localScaling = scaling;
}



void	btConvexShape::getAabbSlow(const btTransform& trans,btVector3&minAabb,btVector3&maxAabb) const
{

	btScalar margin = getMargin();
	for (int i=0;i<3;i++)
	{
		btVector3 vec(0.f,0.f,0.f);
		vec[i] = 1.f;

		btVector3 sv = localGetSupportingVertex(vec*trans.getBasis());

		btVector3 tmp = trans(sv);
		maxAabb[i] = tmp[i]+margin;
		vec[i] = -1.f;
		tmp = trans(localGetSupportingVertex(vec*trans.getBasis()));
		minAabb[i] = tmp[i]-margin;
	}
};

btVector3	btConvexShape::localGetSupportingVertex(const btVector3& vec)const
 {
	 btVector3	supVertex = localGetSupportingVertexWithoutMargin(vec);

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


