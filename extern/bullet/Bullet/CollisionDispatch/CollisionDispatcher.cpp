/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#include "CollisionDispatcher.h"


#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "CollisionDispatch/ConvexConvexAlgorithm.h"
#include "CollisionDispatch/EmptyCollisionAlgorithm.h"
#include "CollisionDispatch/ConvexConcaveCollisionAlgorithm.h"

#include "NarrowPhaseCollision/CollisionObject.h"
#include <algorithm>

void CollisionDispatcher::FindUnions()
{
	if (m_useIslands)
	{
		for (int i=0;i<GetNumManifolds();i++)
		{
			const PersistentManifold* manifold = this->GetManifoldByIndexInternal(i);
			//static objects (invmass 0.f) don't merge !
			if ((((CollisionObject*)manifold->GetBody0()) && (((CollisionObject*)manifold->GetBody0())->mergesSimulationIslands())) &&
				(((CollisionObject*)manifold->GetBody1()) && (((CollisionObject*)manifold->GetBody1())->mergesSimulationIslands())))
			{

				m_unionFind.unite(((CollisionObject*)manifold->GetBody0())->m_islandTag1,
					((CollisionObject*)manifold->GetBody1())->m_islandTag1);
			}
			
			
		}
	}
	
}
	

	
CollisionDispatcher::CollisionDispatcher (): 
	m_useIslands(true),
		m_count(0)
{
	int i;
	
	for (i=0;i<MAX_BROADPHASE_COLLISION_TYPES;i++)
	{
		for (int j=0;j<MAX_BROADPHASE_COLLISION_TYPES;j++)
		{
			m_doubleDispatch[i][j] = 0;
		}
	}
	
	
};
	
PersistentManifold*	CollisionDispatcher::GetNewManifold(void* b0,void* b1) 
{ 

	CollisionObject* body0 = (CollisionObject*)b0;
	CollisionObject* body1 = (CollisionObject*)b1;
	
	PersistentManifold* manifold = new PersistentManifold (body0,body1);
	m_manifoldsPtr.push_back(manifold);

	return manifold;
}

	
void CollisionDispatcher::ReleaseManifold(PersistentManifold* manifold)
{
	manifold->ClearManifold();

	std::vector<PersistentManifold*>::iterator i =
		std::find(m_manifoldsPtr.begin(), m_manifoldsPtr.end(), manifold);
	if (!(i == m_manifoldsPtr.end()))
	{
		std::swap(*i, m_manifoldsPtr.back());
		m_manifoldsPtr.pop_back();
	}
	
	
}

	
//
// todo: this is random access, it can be walked 'cache friendly'!
//
void CollisionDispatcher::BuildAndProcessIslands(int numBodies, IslandCallback* callback)
{
	int i;


	for (int islandId=0;islandId<numBodies;islandId++)
	{

		std::vector<PersistentManifold*>  islandmanifold;
		
		//int numSleeping = 0;

		bool allSleeping = true;

		for (i=0;i<GetNumManifolds();i++)
		{
			 PersistentManifold* manifold = this->GetManifoldByIndexInternal(i);
			if ((((CollisionObject*)manifold->GetBody0()) && ((CollisionObject*)manifold->GetBody0())->m_islandTag1 == (islandId)) ||
				(((CollisionObject*)manifold->GetBody1()) && ((CollisionObject*)manifold->GetBody1())->m_islandTag1 == (islandId)))
			{

				if ((((CollisionObject*)manifold->GetBody0()) && ((CollisionObject*)manifold->GetBody0())->GetActivationState()== ACTIVE_TAG) ||
					(((CollisionObject*)manifold->GetBody1()) && ((CollisionObject*)manifold->GetBody1())->GetActivationState() == ACTIVE_TAG))
				{
					allSleeping = false;
				}
				if ((((CollisionObject*)manifold->GetBody0()) && ((CollisionObject*)manifold->GetBody0())->GetActivationState()== DISABLE_DEACTIVATION) ||
					(((CollisionObject*)manifold->GetBody1()) && ((CollisionObject*)manifold->GetBody1())->GetActivationState() == DISABLE_DEACTIVATION))
				{
					allSleeping = false;
				}

				islandmanifold.push_back(manifold);
			}
		}
		if (allSleeping)
		{
			//tag all as 'ISLAND_SLEEPING'
			for (i=0;i<islandmanifold.size();i++)
			{
				 PersistentManifold* manifold = islandmanifold[i];
				if (((CollisionObject*)manifold->GetBody0()))	
				{
					((CollisionObject*)manifold->GetBody0())->SetActivationState( ISLAND_SLEEPING );
				}
				if (((CollisionObject*)manifold->GetBody1()))	
				{
					((CollisionObject*)manifold->GetBody1())->SetActivationState( ISLAND_SLEEPING);
				}

			}
		} else
		{

			//tag all as 'ISLAND_SLEEPING'
			for (i=0;i<islandmanifold.size();i++)
			{
				 PersistentManifold* manifold = islandmanifold[i];
				 CollisionObject* body0 = (CollisionObject*)manifold->GetBody0();
				 CollisionObject* body1 = (CollisionObject*)manifold->GetBody1();

				if (body0)	
				{
					if ( body0->GetActivationState() == ISLAND_SLEEPING)
					{
						body0->SetActivationState( WANTS_DEACTIVATION);
					}
				}
				if (body1)	
				{
					if ( body1->GetActivationState() == ISLAND_SLEEPING)
					{
						body1->SetActivationState(WANTS_DEACTIVATION);
					}
				}

			}

			callback->ProcessIsland(&islandmanifold[0],islandmanifold.size());

		}
	}
}



CollisionAlgorithm* CollisionDispatcher::InternalFindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
{
	m_count++;
	
	CollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = this;
	
	if (proxy0.IsConvexShape() && proxy1.IsConvexShape() )
	{
		return new ConvexConvexAlgorithm(0,ci,&proxy0,&proxy1);			
	}

	if (proxy0.IsConvexShape() && proxy1.IsConcaveShape())
	{
		return new ConvexConcaveCollisionAlgorithm(ci,&proxy0,&proxy1);
	}

	if (proxy1.IsConvexShape() && proxy0.IsConcaveShape())
	{
		return new ConvexConcaveCollisionAlgorithm(ci,&proxy1,&proxy0);
	}

	//failed to find an algorithm
	return new EmptyAlgorithm(ci);
	
}

bool	CollisionDispatcher::NeedsCollision(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
{

	CollisionObject* body0 = (CollisionObject*)proxy0.m_clientObject;
	CollisionObject* body1 = (CollisionObject*)proxy1.m_clientObject;

	assert(body0);
	assert(body1);

	bool needsCollision = true;

	if ((body0->m_collisionFlags & CollisionObject::isStatic) && 
		(body1->m_collisionFlags & CollisionObject::isStatic))
		needsCollision = false;
	
	if ((body0->GetActivationState() == 2) &&(body1->GetActivationState() == 2))
		needsCollision = false;
	
	return needsCollision ;

}
