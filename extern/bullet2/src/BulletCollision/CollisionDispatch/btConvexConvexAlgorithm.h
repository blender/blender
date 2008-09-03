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

#include "../BroadphaseCollision/btCollisionAlgorithm.h"
#include "../NarrowPhaseCollision/btGjkPairDetector.h"
#include "../NarrowPhaseCollision/btPersistentManifold.h"
#include "../BroadphaseCollision/btBroadphaseProxy.h"
#include "../NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include "btCollisionCreateFunc.h"

class btConvexPenetrationDepthSolver;

///ConvexConvexAlgorithm collision algorithm implements time of impact, convex closest points and penetration depth calculations.
class btConvexConvexAlgorithm : public btCollisionAlgorithm
{
	btGjkPairDetector m_gjkPairDetector;
public:

	bool	m_ownManifold;
	btPersistentManifold*	m_manifoldPtr;
	bool			m_lowLevelOfDetail;
	

public:

	btConvexConvexAlgorithm(btPersistentManifold* mf,const btCollisionAlgorithmConstructionInfo& ci,btCollisionObject* body0,btCollisionObject* body1, btSimplexSolverInterface* simplexSolver, btConvexPenetrationDepthSolver* pdSolver);

	virtual ~btConvexConvexAlgorithm();

	virtual void processCollision (btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	virtual btScalar calculateTimeOfImpact(btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	void	setLowLevelOfDetail(bool useLowLevel);


	const btPersistentManifold*	getManifold()
	{
		return m_manifoldPtr;
	}

	struct CreateFunc :public 	btCollisionAlgorithmCreateFunc
	{
		btConvexPenetrationDepthSolver*		m_pdSolver;
		btSimplexSolverInterface*			m_simplexSolver;
		bool	m_ownsSolvers;
		
		CreateFunc(btSimplexSolverInterface*			simplexSolver, btConvexPenetrationDepthSolver* pdSolver);
		CreateFunc();
		virtual ~CreateFunc();

		virtual	btCollisionAlgorithm* CreateCollisionAlgorithm(btCollisionAlgorithmConstructionInfo& ci, btCollisionObject* body0,btCollisionObject* body1)
		{
			return new btConvexConvexAlgorithm(ci.m_manifold,ci,body0,body1,m_simplexSolver,m_pdSolver);
		}
	};


};

#endif //CONVEX_CONVEX_ALGORITHM_H
