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
	//ConvexPenetrationDepthSolver*	m_penetrationDepthSolver;
	VoronoiSimplexSolver	m_simplexSolver;
	GjkPairDetector m_gjkPairDetector;
	bool	m_useEpa;
public:
	BroadphaseProxy	m_box0;
	BroadphaseProxy	m_box1;

	bool	m_ownManifold;
	PersistentManifold*	m_manifoldPtr;
	bool			m_lowLevelOfDetail;

	void	CheckPenetrationDepthSolver();

	

public:

	ConvexConvexAlgorithm(PersistentManifold* mf,const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	virtual ~ConvexConvexAlgorithm();

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

	virtual float CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

	void	SetLowLevelOfDetail(bool useLowLevel);

	

	const PersistentManifold*	GetManifold()
	{
		return m_manifoldPtr;
	}

};

#endif //CONVEX_CONVEX_ALGORITHM_H
