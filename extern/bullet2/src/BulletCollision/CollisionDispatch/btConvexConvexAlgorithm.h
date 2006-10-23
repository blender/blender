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

#include "BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h"
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include "btCollisionCreateFunc.h"

class btConvexPenetrationDepthSolver;

///ConvexConvexAlgorithm collision algorithm implements time of impact, convex closest points and penetration depth calculations.
class btConvexConvexAlgorithm : public btCollisionAlgorithm
{
	//ConvexPenetrationDepthSolver*	m_penetrationDepthSolver;
	btVoronoiSimplexSolver	m_simplexSolver;
	btGjkPairDetector m_gjkPairDetector;
	bool	m_useEpa;
public:

	bool	m_ownManifold;
	btPersistentManifold*	m_manifoldPtr;
	bool			m_lowLevelOfDetail;

	void	checkPenetrationDepthSolver();

	

public:

	btConvexConvexAlgorithm(btPersistentManifold* mf,const btCollisionAlgorithmConstructionInfo& ci,btCollisionObject* body0,btCollisionObject* body1);

	virtual ~btConvexConvexAlgorithm();

	virtual void processCollision (btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	virtual float calculateTimeOfImpact(btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	void	setLowLevelOfDetail(bool useLowLevel);

	virtual void setShapeIdentifiers(int partId0,int index0,	int partId1,int index1)
	{
			m_gjkPairDetector.m_partId0=partId0;
			m_gjkPairDetector.m_partId1=partId1;
			m_gjkPairDetector.m_index0=index0;
			m_gjkPairDetector.m_index1=index1;		
	}

	const btPersistentManifold*	getManifold()
	{
		return m_manifoldPtr;
	}

	struct CreateFunc :public 	btCollisionAlgorithmCreateFunc
	{
		virtual	btCollisionAlgorithm* CreateCollisionAlgorithm(btCollisionAlgorithmConstructionInfo& ci, btCollisionObject* body0,btCollisionObject* body1)
		{
			return new btConvexConvexAlgorithm(0,ci,body0,body1);
		}
	};


};

#endif //CONVEX_CONVEX_ALGORITHM_H
