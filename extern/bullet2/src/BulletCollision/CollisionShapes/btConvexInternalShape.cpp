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


#include "btConvexInternalShape.h"


btConvexInternalShape::btConvexInternalShape()
: m_localScaling(btScalar(1.),btScalar(1.),btScalar(1.)),
m_collisionMargin(CONVEX_DISTANCE_MARGIN)
{
}


void	btConvexInternalShape::setLocalScaling(const btVector3& scaling)
{
	m_localScaling = scaling.absolute();
}



void	btConvexInternalShape::getAabbSlow(const btTransform& trans,btVector3&minAabb,btVector3&maxAabb) const
{

	btScalar margin = getMargin();
	for (int i=0;i<3;i++)
	{
		btVector3 vec(btScalar(0.),btScalar(0.),btScalar(0.));
		vec[i] = btScalar(1.);

		btVector3 sv = localGetSupportingVertex(vec*trans.getBasis());

		btVector3 tmp = trans(sv);
		maxAabb[i] = tmp[i]+margin;
		vec[i] = btScalar(-1.);
		tmp = trans(localGetSupportingVertex(vec*trans.getBasis()));
		minAabb[i] = tmp[i]-margin;
	}
};


btVector3	btConvexInternalShape::localGetSupportingVertex(const btVector3& vec)const
{
#ifndef __SPU__

	 btVector3	supVertex = localGetSupportingVertexWithoutMargin(vec);

	if ( getMargin()!=btScalar(0.) )
	{
		btVector3 vecnorm = vec;
		if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
		{
			vecnorm.setValue(btScalar(-1.),btScalar(-1.),btScalar(-1.));
		} 
		vecnorm.normalize();
		supVertex+= getMargin() * vecnorm;
	}
	return supVertex;

#else
	return btVector3(0,0,0);
#endif //__SPU__

 }


