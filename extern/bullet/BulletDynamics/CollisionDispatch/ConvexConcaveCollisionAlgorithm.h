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
#ifndef CONVEX_CONCAVE_COLLISION_ALGORITHM_H
#define CONVEX_CONCAVE_COLLISION_ALGORITHM_H

#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "BroadphaseCollision/CollisionDispatcher.h"
#include "BroadphaseCollision/BroadphaseInterface.h"
#include "CollisionShapes/TriangleCallback.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
class Dispatcher;
#include "BroadphaseCollision/BroadphaseProxy.h"



class BoxTriangleCallback : public TriangleCallback
{
	BroadphaseProxy* m_boxProxy;
	BroadphaseProxy m_triangleProxy;

	SimdVector3	m_aabbMin;
	SimdVector3	m_aabbMax ;

	Dispatcher*	m_dispatcher;
	float	m_timeStep;
	int	m_stepCount;
	bool m_useContinuous;
	
public:

	PersistentManifold*	m_manifoldPtr;

	BoxTriangleCallback(Dispatcher* dispatcher,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	void	SetTimeStepAndCounters(float timeStep,int stepCount, bool useContinuous);

	virtual ~BoxTriangleCallback();

	virtual void ProcessTriangle(SimdVector3* triangle);
	
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

	BoxTriangleCallback m_boxTriangleCallback;


public:

	ConvexConcaveCollisionAlgorithm( const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	virtual ~ConvexConcaveCollisionAlgorithm();

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount, bool useContinuous);

	float	CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount);

	void	ClearCache();

};

#endif //CONVEX_CONCAVE_COLLISION_ALGORITHM_H
