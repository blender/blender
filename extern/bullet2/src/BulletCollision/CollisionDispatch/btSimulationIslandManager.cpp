

#include "LinearMath/btScalar.h"
#include "btSimulationIslandManager.h"
#include "BulletCollision/BroadphaseCollision/btDispatcher.h"
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"

#include <stdio.h>
#include <algorithm>


btSimulationIslandManager::btSimulationIslandManager()
{
}

btSimulationIslandManager::~btSimulationIslandManager()
{
}


void btSimulationIslandManager::initUnionFind(int n)
{
		m_unionFind.reset(n);
}
		

void btSimulationIslandManager::findUnions(btDispatcher* dispatcher)
{
	
	{
		for (int i=0;i<dispatcher->getNumManifolds();i++)
		{
			const btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
			//static objects (invmass 0.f) don't merge !

			 const  btCollisionObject* colObj0 = static_cast<const btCollisionObject*>(manifold->getBody0());
			 const  btCollisionObject* colObj1 = static_cast<const btCollisionObject*>(manifold->getBody1());

			if (((colObj0) && ((colObj0)->mergesSimulationIslands())) &&
				((colObj1) && ((colObj1)->mergesSimulationIslands())))
			{

				m_unionFind.unite((colObj0)->getIslandTag(),
					(colObj1)->getIslandTag());
			}
		}
	}
}


void	btSimulationIslandManager::updateActivationState(btCollisionWorld* colWorld,btDispatcher* dispatcher)
{
	
	initUnionFind( int (colWorld->getCollisionObjectArray().size()));
	
	// put the index into m_controllers into m_tag	
	{
		std::vector<btCollisionObject*>::iterator i;
		
		int index = 0;
		for (i=colWorld->getCollisionObjectArray().begin();
		!(i==colWorld->getCollisionObjectArray().end()); i++)
		{
			
			btCollisionObject*	collisionObject= (*i);
			collisionObject->setIslandTag(index);
			collisionObject->setHitFraction(1.f);
			index++;
			
		}
	}
	// do the union find
	
	findUnions(dispatcher);
	

	
}




void	btSimulationIslandManager::storeIslandActivationState(btCollisionWorld* colWorld)
{
	// put the islandId ('find' value) into m_tag	
	{
		
		
		std::vector<btCollisionObject*>::iterator i;
		
		int index = 0;
		for (i=colWorld->getCollisionObjectArray().begin();
		!(i==colWorld->getCollisionObjectArray().end()); i++)
		{
			btCollisionObject* collisionObject= (*i);
			
			if (collisionObject->mergesSimulationIslands())
			{
				collisionObject->setIslandTag( m_unionFind.find(index) );
			} else
			{
				collisionObject->setIslandTag(-1);
			}
			index++;
		}
	}
}

inline	int	getIslandId(const btPersistentManifold* lhs)
{
	int islandId;
	const btCollisionObject* rcolObj0 = static_cast<const btCollisionObject*>(lhs->getBody0());
	const btCollisionObject* rcolObj1 = static_cast<const btCollisionObject*>(lhs->getBody1());
	islandId= rcolObj0->getIslandTag()>=0?rcolObj0->getIslandTag():rcolObj1->getIslandTag();
	return islandId;

}

bool btPersistentManifoldSortPredicate(const btPersistentManifold* lhs, const btPersistentManifold* rhs)
{
	int rIslandId0,lIslandId0;
	rIslandId0 = getIslandId(rhs);
	lIslandId0 = getIslandId(lhs);
	return lIslandId0 < rIslandId0;
}


//
// todo: this is random access, it can be walked 'cache friendly'!
//
void btSimulationIslandManager::buildAndProcessIslands(btDispatcher* dispatcher,btCollisionObjectArray& collisionObjects, IslandCallback* callback)
{
	//we are going to sort the unionfind array, and store the element id in the size
	//afterwards, we clean unionfind, to make sure no-one uses it anymore
	
	getUnionFind().sortIslands();
	int numElem = getUnionFind().getNumElements();

	int endIslandIndex=1;

	//update the sleeping state for bodies, if all are sleeping
	for (int startIslandIndex=0;startIslandIndex<numElem;startIslandIndex = endIslandIndex)
	{
		int islandId = getUnionFind().getElement(startIslandIndex).m_id;
		for (endIslandIndex = startIslandIndex+1;(endIslandIndex<numElem) && (getUnionFind().getElement(endIslandIndex).m_id == islandId);endIslandIndex++)
		{
		}

		//int numSleeping = 0;

		bool allSleeping = true;

		int idx;
		for (idx=startIslandIndex;idx<endIslandIndex;idx++)
		{
			int i = getUnionFind().getElement(idx).m_sz;

			btCollisionObject* colObj0 = collisionObjects[i];
			if ((colObj0->getIslandTag() != islandId) && (colObj0->getIslandTag() != -1))
			{
				printf("error in island management\n");
			}

			assert((colObj0->getIslandTag() == islandId) || (colObj0->getIslandTag() == -1));
			if (colObj0->getIslandTag() == islandId)
			{
				if (colObj0->getActivationState()== ACTIVE_TAG)
				{
					allSleeping = false;
				}
				if (colObj0->getActivationState()== DISABLE_DEACTIVATION)
				{
					allSleeping = false;
				}
			}
		}
			

		if (allSleeping)
		{
			int idx;
			for (idx=startIslandIndex;idx<endIslandIndex;idx++)
			{
				int i = getUnionFind().getElement(idx).m_sz;
				btCollisionObject* colObj0 = collisionObjects[i];
				if ((colObj0->getIslandTag() != islandId) && (colObj0->getIslandTag() != -1))
				{
					printf("error in island management\n");
				}

				assert((colObj0->getIslandTag() == islandId) || (colObj0->getIslandTag() == -1));

				if (colObj0->getIslandTag() == islandId)
				{
					colObj0->setActivationState( ISLAND_SLEEPING );
				}
			}
		} else
		{

			int idx;
			for (idx=startIslandIndex;idx<endIslandIndex;idx++)
			{
				int i = getUnionFind().getElement(idx).m_sz;

				btCollisionObject* colObj0 = collisionObjects[i];
				if ((colObj0->getIslandTag() != islandId) && (colObj0->getIslandTag() != -1))
				{
					printf("error in island management\n");
				}

				assert((colObj0->getIslandTag() == islandId) || (colObj0->getIslandTag() == -1));

				if (colObj0->getIslandTag() == islandId)
				{
					if ( colObj0->getActivationState() == ISLAND_SLEEPING)
					{
						colObj0->setActivationState( WANTS_DEACTIVATION);
					}
				}
			}
		}
	}

	std::vector<btPersistentManifold*>  islandmanifold;
	int i;
	int maxNumManifolds = dispatcher->getNumManifolds();
	islandmanifold.reserve(maxNumManifolds);

	for (i=0;i<maxNumManifolds ;i++)
	{
		 btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
		 
		 btCollisionObject* colObj0 = static_cast<btCollisionObject*>(manifold->getBody0());
		 btCollisionObject* colObj1 = static_cast<btCollisionObject*>(manifold->getBody1());
		
		 //todo: check sleeping conditions!
		 if (((colObj0) && colObj0->getActivationState() != ISLAND_SLEEPING) ||
			((colObj1) && colObj1->getActivationState() != ISLAND_SLEEPING))
		{
		
			//kinematic objects don't merge islands, but wake up all connected objects
			if (colObj0->isStaticOrKinematicObject() && colObj0->getActivationState() != ISLAND_SLEEPING)
			{
				colObj1->activate();
			}
			if (colObj1->isStaticOrKinematicObject() && colObj1->getActivationState() != ISLAND_SLEEPING)
			{
				colObj0->activate();
			}

			//filtering for response
			if (dispatcher->needsResponse(colObj0,colObj1))
				islandmanifold.push_back(manifold);
		}
	}

	int numManifolds = int (islandmanifold.size());

	// Sort manifolds, based on islands
	// Sort the vector using predicate and std::sort
	std::sort(islandmanifold.begin(), islandmanifold.end(), btPersistentManifoldSortPredicate);

	//now process all active islands (sets of manifolds for now)

	int startManifoldIndex = 0;
	int endManifoldIndex = 1;

	int islandId;


		//update the sleeping state for bodies, if all are sleeping
	for (int startIslandIndex=0;startIslandIndex<numElem;startIslandIndex = endIslandIndex)
	{
		int islandId = getUnionFind().getElement(startIslandIndex).m_id;

		for (endIslandIndex = startIslandIndex+1;(endIslandIndex<numElem) && (getUnionFind().getElement(endIslandIndex).m_id == islandId);endIslandIndex++)
		{
		}

		//find the accompanying contact manifold for this islandId
		int numIslandManifolds = 0;
		btPersistentManifold** startManifold = 0;

		if (startManifoldIndex<numManifolds)
		{
			int curIslandId = getIslandId(islandmanifold[startManifoldIndex]);
			if (curIslandId == islandId)
			{
				startManifold = &islandmanifold[startManifoldIndex];
			
				for (endManifoldIndex = startManifoldIndex+1;(endManifoldIndex<numManifolds) && (islandId == getIslandId(islandmanifold[endManifoldIndex]));endManifoldIndex++)
				{

				}
				/// Process the actual simulation, only if not sleeping/deactivated
				numIslandManifolds = endManifoldIndex-startManifoldIndex;
			}

		}

		callback->ProcessIsland(startManifold,numIslandManifolds, islandId);

		if (numIslandManifolds)
		{
			startManifoldIndex = endManifoldIndex;
		}
	}
}
