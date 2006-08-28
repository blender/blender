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

#ifndef CONVEX_CONCAVE_COLLISION_ALGORITHM_H
#define CONVEX_CONCAVE_COLLISION_ALGORITHM_H

#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "BroadphaseCollision/Dispatcher.h"
#include "BroadphaseCollision/BroadphaseInterface.h"
#include "CollisionShapes/TriangleCallback.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
class Dispatcher;
#include "BroadphaseCollision/BroadphaseProxy.h"


///For each triangle in the concave mesh that overlaps with the AABB of a convex (m_convexProxy), ProcessTriangle is called.
class ConvexTriangleCallback : public TriangleCallback
{
	BroadphaseProxy* m_convexProxy;
	BroadphaseProxy m_triangleProxy;

	SimdVector3	m_aabbMin;
	SimdVector3	m_aabbMax ;

	Dispatcher*	m_dispatcher;
	const DispatcherInfo* m_dispatchInfoPtr;
	float m_collisionMarginTriangle;
	
public:
int	m_triangleCount;
	
	PersistentManifold*	m_manifoldPtr;

	ConvexTriangleCallback(Dispatcher* dispatcher,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	void	SetTimeStepAndCounters(float collisionMarginTriangle,const DispatcherInfo& dispatchInfo);

	virtual ~ConvexTriangleCallback();

	virtual void ProcessTriangle(SimdVector3* triangle, int partId, int triangleIndex);
	
	void ClearCache();

	inline const SimdVector3& GetAabbMin() const
	{
		return m_aabbMin;
	}
	inline const SimdVector3& GetAabbMax() const
	{
		return m_aabbMax;
	}

};




/// ConvexConcaveCollisionAlgorithm  supports collision between convex shapes and (concave) trianges meshes.
class ConvexConcaveCollisionAlgorithm  : public CollisionAlgorithm
{

	BroadphaseProxy m_convex;

	BroadphaseProxy m_concave;

	ConvexTriangleCallback m_ConvexTriangleCallback;


public:

	ConvexConcaveCollisionAlgorithm( const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	virtual ~ConvexConcaveCollisionAlgorithm();

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

	float	CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

	void	ClearCache();

};

#endif //CONVEX_CONCAVE_COLLISION_ALGORITHM_H
