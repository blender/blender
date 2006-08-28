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



#include "ParallelPhysicsEnvironment.h"
#include "CcdPhysicsController.h"
#include "ParallelIslandDispatcher.h"
#include "CollisionDispatch/CollisionWorld.h"
#include "ConstraintSolver/TypedConstraint.h"
#include "CollisionDispatch/SimulationIslandManager.h"
#include "SimulationIsland.h"


ParallelPhysicsEnvironment::ParallelPhysicsEnvironment(ParallelIslandDispatcher* dispatcher, OverlappingPairCache* pairCache):
CcdPhysicsEnvironment(dispatcher,pairCache)
{
	
}

ParallelPhysicsEnvironment::~ParallelPhysicsEnvironment()
{

}



/// Perform an integration step of duration 'timeStep'.
bool	ParallelPhysicsEnvironment::proceedDeltaTimeOneStep(float timeStep)
{
	// Make sure the broadphase / overlapping AABB paircache is up-to-date
	OverlappingPairCache*	scene = m_collisionWorld->GetPairCache();
	scene->RefreshOverlappingPairs();

	// Find the connected sets that can be simulated in parallel
	// Using union find

#ifdef USE_QUICKPROF
	Profiler::beginBlock("IslandUnionFind");
#endif //USE_QUICKPROF

	GetSimulationIslandManager()->UpdateActivationState(GetCollisionWorld(),GetCollisionWorld()->GetDispatcher());

	{
		int i;
		int numConstraints = m_constraints.size();
		for (i=0;i< numConstraints ; i++ )
		{
			TypedConstraint* constraint = m_constraints[i];

			const RigidBody* colObj0 = &constraint->GetRigidBodyA();
			const RigidBody* colObj1 = &constraint->GetRigidBodyB();

			if (((colObj0) && ((colObj0)->mergesSimulationIslands())) &&
				((colObj1) && ((colObj1)->mergesSimulationIslands())))
			{
				if (colObj0->IsActive() || colObj1->IsActive())
				{

					GetSimulationIslandManager()->GetUnionFind().unite((colObj0)->m_islandTag1,
						(colObj1)->m_islandTag1);
				}
			}
		}
	}

	//Store the island id in each body
	GetSimulationIslandManager()->StoreIslandActivationState(GetCollisionWorld());

#ifdef USE_QUICKPROF
	Profiler::endBlock("IslandUnionFind");
#endif //USE_QUICKPROF

	

	///build simulation islands
	
#ifdef USE_QUICKPROF
	Profiler::beginBlock("BuildIslands");
#endif //USE_QUICKPROF

	std::vector<SimulationIsland> simulationIslands;
	simulationIslands.resize(GetNumControllers());

	int k;
	for (k=0;k<GetNumControllers();k++)
	{
			CcdPhysicsController* ctrl = m_controllers[k];
			int tag = ctrl->GetRigidBody()->m_islandTag1;
			if (tag>=0)
			{
				simulationIslands[tag].m_controllers.push_back(ctrl);
			}
	}

	Dispatcher* dispatcher = GetCollisionWorld()->GetDispatcher();

	
	//this is a brute force approach, will rethink later about more subtle ways
	int i;
	for (i=0;i<	scene->GetNumOverlappingPairs();i++)
	{
		BroadphasePair* pair = &scene->GetOverlappingPair(i);

		CollisionObject*	col0 = static_cast<CollisionObject*>(pair->m_pProxy0->m_clientObject);
		CollisionObject*	col1 = static_cast<CollisionObject*>(pair->m_pProxy1->m_clientObject);
		
		if (col0->m_islandTag1 > col1->m_islandTag1)
		{
			simulationIslands[col0->m_islandTag1].m_overlappingPairIndices.push_back(i);
		} else
		{
			simulationIslands[col1->m_islandTag1].m_overlappingPairIndices.push_back(i);
		}
	}
	
	//store constraint indices for each island
	for (i=0;i<m_constraints.size();i++)
	{
		TypedConstraint& constraint = *m_constraints[i];
		if (constraint.GetRigidBodyA().m_islandTag1 > constraint.GetRigidBodyB().m_islandTag1)
		{
			simulationIslands[constraint.GetRigidBodyA().m_islandTag1].m_constraintIndices.push_back(i);
		} else
		{
			simulationIslands[constraint.GetRigidBodyB().m_islandTag1].m_constraintIndices.push_back(i);
		}

	}

	//add all overlapping pairs for each island

	for (i=0;i<dispatcher->GetNumManifolds();i++)
	{
		 PersistentManifold* manifold = dispatcher->GetManifoldByIndexInternal(i);
		 
		 //filtering for response

		 CollisionObject* colObj0 = static_cast<CollisionObject*>(manifold->GetBody0());
		 CollisionObject* colObj1 = static_cast<CollisionObject*>(manifold->GetBody1());
		 {
			 int islandTag = colObj0->m_islandTag1;
			 if (colObj1->m_islandTag1 > islandTag)
				 islandTag = colObj1->m_islandTag1;

				if (dispatcher->NeedsResponse(*colObj0,*colObj1))
					simulationIslands[islandTag].m_manifolds.push_back(manifold);
			
		 }
	}
		
	#ifdef USE_QUICKPROF
		Profiler::endBlock("BuildIslands");
	#endif //USE_QUICKPROF


#ifdef USE_QUICKPROF
	Profiler::beginBlock("SimulateIsland");
#endif //USE_QUICKPROF
	
	TypedConstraint** constraintBase = 0;
	if (m_constraints.size())
		constraintBase = &m_constraints[0];



	//Each simulation island can be processed in parallel (will be put on a job queue)
	for (k=0;k<simulationIslands.size();k++)
	{
		if (simulationIslands[k].m_controllers.size())
		{
			simulationIslands[k].Simulate(m_debugDrawer,m_numIterations, constraintBase ,&scene->GetOverlappingPair(0),dispatcher,GetBroadphase(),m_solver,timeStep);
		}
	}

#ifdef USE_QUICKPROF
	Profiler::endBlock("SimulateIsland");
#endif //USE_QUICKPROF

	return true;

}