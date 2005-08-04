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
#ifndef CONVEX_CONVEX_ALGORITHM_H
#define CONVEX_CONVEX_ALGORITHM_H

#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "NarrowPhaseCollision/GjkPairDetector.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include "NarrowPhaseCollision/VoronoiSimplexSolver.h"

class ConvexPenetrationDepthSolver;

///ConvexConvexAlgorithm collision algorithm implements time of impact, convex closest points and penetration depth calculations.
class ConvexConvexAlgorithm : public CollisionAlgorithm
{
	ConvexPenetrationDepthSolver*	m_penetrationDepthSolver;
	VoronoiSimplexSolver	m_simplexSolver;
	GjkPairDetector m_gjkPairDetector;
	bool	m_useEpa;
public:
	BroadphaseProxy	m_box0;
	BroadphaseProxy	m_box1;
	float			m_collisionImpulse;
	bool	m_ownManifold;
	PersistentManifold*	m_manifoldPtr;
	bool			m_lowLevelOfDetail;

	void	CheckPenetrationDepthSolver();

	

public:

	ConvexConvexAlgorithm(PersistentManifold* mf,const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	virtual ~ConvexConvexAlgorithm();

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount, bool useContinuous);

	virtual float CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount);

	void	SetLowLevelOfDetail(bool useLowLevel);

	float	GetCollisionImpulse() const;

	const PersistentManifold*	GetManifold()
	{
		return m_manifoldPtr;
	}

};

#endif //CONVEX_CONVEX_ALGORITHM_H
