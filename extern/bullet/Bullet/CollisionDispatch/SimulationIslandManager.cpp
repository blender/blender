
#include "SimulationIslandManager.h"
#include "BroadphaseCollision/Dispatcher.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "CollisionDispatch/CollisionObject.h"
#include "CollisionDispatch/CollisionWorld.h"



SimulationIslandManager::SimulationIslandManager()
{
}

void SimulationIslandManager::InitUnionFind(int n)
{
		m_unionFind.reset(n);
}
		

void SimulationIslandManager::FindUnions(Dispatcher* dispatcher)
{
	
	{
		for (int i=0;i<dispatcher->GetNumManifolds();i++)
		{
			const PersistentManifold* manifold = dispatcher->GetManifoldByIndexInternal(i);
			//static objects (invmass 0.f) don't merge !

			 const  CollisionObject* colObj0 = static_cast<const CollisionObject*>(manifold->GetBody0());
			 const  CollisionObject* colObj1 = static_cast<const CollisionObject*>(manifold->GetBody1());

			 if (colObj0 && colObj1 && dispatcher->NeedsResponse(*colObj0,*colObj1))
			 {
				if (((colObj0) && ((colObj0)->mergesSimulationIslands())) &&
					((colObj1) && ((colObj1)->mergesSimulationIslands())))
				{

					m_unionFind.unite((colObj0)->m_islandTag1,
						(colObj1)->m_islandTag1);
				}
			 }
			
			
		}
	}
	
}


void	SimulationIslandManager::UpdateActivationState(CollisionWorld* colWorld,Dispatcher* dispatcher)
{
	
	InitUnionFind(colWorld->GetCollisionObjectArray().size());
	
	// put the index into m_controllers into m_tag	
	{
		std::vector<CollisionObject*>::iterator i;
		
		int index = 0;
		for (i=colWorld->GetCollisionObjectArray().begin();
		!(i==colWorld->GetCollisionObjectArray().end()); i++)
		{
			
			CollisionObject*	collisionObject= (*i);
			collisionObject->m_islandTag1 = index;
			collisionObject->m_hitFraction = 1.f;
			index++;
			
		}
	}
	// do the union find
	
	FindUnions(dispatcher);
	

	
}




void	SimulationIslandManager::StoreIslandActivationState(CollisionWorld* colWorld)
{
	// put the islandId ('find' value) into m_tag	
	{
		
		
		std::vector<CollisionObject*>::iterator i;
		
		int index = 0;
		for (i=colWorld->GetCollisionObjectArray().begin();
		!(i==colWorld->GetCollisionObjectArray().end()); i++)
		{
			CollisionObject* collisionObject= (*i);
			
			if (collisionObject->mergesSimulationIslands())
			{
				collisionObject->m_islandTag1 = m_unionFind.find(index);
			} else
			{
				collisionObject->m_islandTag1 = -1;
			}
			index++;
		}
	}
}




//
// todo: this is random access, it can be walked 'cache friendly'!
//
void SimulationIslandManager::BuildAndProcessIslands(Dispatcher* dispatcher,CollisionObjectArray& collisionObjects, IslandCallback* callback)
{
	
	int numBodies  = collisionObjects.size();

	//first calculate the number of islands, and iterate over the islands id's

	const UnionFind& uf = this->GetUnionFind();

	for (int islandId=0;islandId<uf.getNumElements();islandId++)
	{
		if (uf.isRoot(islandId))
		{

			std::vector<PersistentManifold*>  islandmanifold;
			
			//int numSleeping = 0;

			bool allSleeping = true;

			int i;
			for (i=0;i<numBodies;i++)
			{
				CollisionObject* colObj0 = collisionObjects[i];
			
				if (colObj0->m_islandTag1 == islandId)
				{
				
					if (colObj0->GetActivationState()== ACTIVE_TAG)
					{
						allSleeping = false;
					}
					if (colObj0->GetActivationState()== DISABLE_DEACTIVATION)
					{
						allSleeping = false;
					}
				}
			}

			
			for (i=0;i<dispatcher->GetNumManifolds();i++)
			{
				 PersistentManifold* manifold = dispatcher->GetManifoldByIndexInternal(i);
				 
				 //filtering for response

				 CollisionObject* colObj0 = static_cast<CollisionObject*>(manifold->GetBody0());
				 CollisionObject* colObj1 = static_cast<CollisionObject*>(manifold->GetBody1());
				 assert(colObj0);
				 assert(colObj1);
				 {
					if (((colObj0)->m_islandTag1 == (islandId)) ||
						((colObj1)->m_islandTag1 == (islandId)))
					{
						if (dispatcher->NeedsResponse(*colObj0,*colObj1))
							islandmanifold.push_back(manifold);
					}
				 }
			}
			if (allSleeping)
			{
				int i;
				for (i=0;i<numBodies;i++)
				{
					CollisionObject* colObj0 = collisionObjects[i];
					if (colObj0->m_islandTag1 == islandId)
					{
						colObj0->SetActivationState( ISLAND_SLEEPING );
					}
				}

				
			} else
			{

				int i;
				for (i=0;i<numBodies;i++)
				{
					CollisionObject* colObj0 = collisionObjects[i];
					if (colObj0->m_islandTag1 == islandId)
					{
						if ( colObj0->GetActivationState() == ISLAND_SLEEPING)
						{
							colObj0->SetActivationState( WANTS_DEACTIVATION);
						}
					}
				}

				/// Process the actual simulation, only if not sleeping/deactivated
				if (islandmanifold.size())
				{
					callback->ProcessIsland(&islandmanifold[0],islandmanifold.size());
				}

			}
		}
	}
}
