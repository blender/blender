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

#ifndef CONVEX_TRIANGLE_CALLBACK_H
#define CONVEX_TRIANGLE_CALLBACK_H

#include "TriangleCallback.h"
class ConvexShape;
class PersistentManifold;
#include "SimdTransform.h"
///ConvexTriangleCallback processes the narrowphase convex-triangle collision detection
class ConvexTriangleCallback: public TriangleCallback
{
	SimdVector3	m_aabbMin;
	SimdVector3	m_aabbMax ;

	SimdTransform	m_triangleMeshTransform;
	SimdTransform	m_convexTransform;

//	bool m_useContinuous;
	float m_collisionMarginTriangle;
	
public:
int	m_triangleCount;
	
	ConvexShape*	m_convexShape;

	PersistentManifold*	m_manifoldPtr;

	ConvexTriangleCallback(PersistentManifold* manifold,ConvexShape* convexShape,const SimdTransform&convexTransform,const SimdTransform& triangleMeshTransform);

	void	Update(float collisionMarginTriangle);

	virtual ~ConvexTriangleCallback();

	virtual void ProcessTriangle(SimdVector3* triangle);
	

	inline const SimdVector3& GetAabbMin() const
	{
		return m_aabbMin;
	}
	inline const SimdVector3& GetAabbMax() const
	{
		return m_aabbMax;
	}

};


#endif //CONVEX_TRIANGLE_CALLBACK_H

