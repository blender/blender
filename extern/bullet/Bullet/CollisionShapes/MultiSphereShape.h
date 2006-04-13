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

#ifndef MULTI_SPHERE_MINKOWSKI_H
#define MULTI_SPHERE_MINKOWSKI_H

#include "ConvexShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

#define MAX_NUM_SPHERES 5

///MultiSphereShape represents implicit convex hull of a collection of spheres (using getSupportingVertex)
class MultiSphereShape : public ConvexShape

{
	
	SimdVector3 m_localPositions[MAX_NUM_SPHERES];
	SimdScalar  m_radi[MAX_NUM_SPHERES];
	SimdVector3	m_inertiaHalfExtents;

	int m_numSpheres;
	float m_minRadius;





public:
	MultiSphereShape (const SimdVector3& inertiaHalfExtents,const SimdVector3* positions,const SimdScalar* radi,int numSpheres);

	///CollisionShape Interface
	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	/// ConvexShape Interface
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;

	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const;
	

	virtual int	GetShapeType() const { return MULTI_SPHERE_SHAPE_PROXYTYPE; }

	virtual char*	GetName()const 
	{
		return "MultiSphere";
	}

};


#endif //MULTI_SPHERE_MINKOWSKI_H
