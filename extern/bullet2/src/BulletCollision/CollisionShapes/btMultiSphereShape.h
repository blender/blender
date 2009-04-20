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

#include "btConvexInternalShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" // for the types

#define MAX_NUM_SPHERES 5

///The btMultiSphereShape represents the convex hull of a collection of spheres. You can create special capsules or other smooth volumes.
///It is possible to animate the spheres for deformation.
class btMultiSphereShape : public btConvexInternalShape

{
	
	btVector3 m_localPositions[MAX_NUM_SPHERES];
	btScalar  m_radi[MAX_NUM_SPHERES];
	btVector3	m_inertiaHalfExtents;

	int m_numSpheres;
	



public:
	btMultiSphereShape (const btVector3& inertiaHalfExtents,const btVector3* positions,const btScalar* radi,int numSpheres);

	///CollisionShape Interface
	virtual void	calculateLocalInertia(btScalar mass,btVector3& inertia) const;

	/// btConvexShape Interface
	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec)const;

	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;
	
	int	getSphereCount() const
	{
		return m_numSpheres;
	}

	const btVector3&	getSpherePosition(int index) const
	{
		return m_localPositions[index];
	}

	btScalar	getSphereRadius(int index) const
	{
		return m_radi[index];
	}


	virtual const char*	getName()const 
	{
		return "MultiSphere";
	}

};


#endif //MULTI_SPHERE_MINKOWSKI_H
