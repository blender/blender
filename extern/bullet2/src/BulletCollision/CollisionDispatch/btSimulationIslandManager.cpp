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


#include "LinearMath/btScalar.h"
#include "btSimulationIslandManager.h"
#include "BulletCollision/BroadphaseCollision/btDispatcher.h"
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"

//#include <stdio.h>
#include "LinearMath/btQuickprof.h"

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
		

void btSimulationIslandManager::findUnions(btDispatcher* /* dispatcher */,btCollisionWorld* colWorld)
{
	
	{
		btBroadphasePair* pairPtr = colWorld->getPairCache()->getOverlappingPairArrayPtr();

		for (int i=0;i<colWorld->getPairCache()->getNumOverlappingPairs();i++)
		{
			const btBroadphasePair& collisionPair = pairPtr[i];
			btCollisionObject* colObj0 = (btCollisionObject*)collisionPair.m_pProxy0->m_clientObject;
			btCollisionObject* colObj1 = (btCollisionObject*)collisionPair.m_pProxy1->m_clientObject;

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
		
		int index = 0;
		int i;
		for (i=0;i<colWorld->getCollisionObjectArray().size(); i++)
		{
			btCollisionObject*	collisionObject= colWorld->getCollisionObjectArray()[i];
			collisionObject->setIslandTag(index);
			collisionObject->setCompanionId(-1);
			collisionObject->setHitFraction(btScalar(1.));
			index++;
			
		}
	}
	// do the union find
	
	findUnions(dispatcher,colWorld);
	

	
}




void	btSimulationIslandManager::storeIslandActivationState(btCollisionWorld* colWorld)
{
	// put the islandId ('find' value) into m_tag	
	{
		
		
		int index = 0;
		int i;
		for (i=0;i<colWorld->getCollisionObjectArray().size();i++)
		{
			btCollisionObject* collisionObject= colWorld->getCollisionObjectArray()[i];
			if (!collisionObject->isStaticOrKinematicObject())
			{
				collisionObject->setIslandTag( m_unionFind.find(index) );
				collisionObject->setCompanionId(-1);
			} else
			{
				collisionObject->setIslandTag(-1);
				collisionObject->setCompanionId(-2);
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



/// function object that routes calls to operator<
class btPersistentManifoldSortPredicate
{
	public:

		SIMD_FORCE_INLINE bool operator() ( const btPersistentManifold* lhs, const btPersistentManifold* rhs )
		{
			return getIslandId(lhs) < getIslandId(rhs);
		}
};


void btSimulationIslandManager::buildIslands(btDispatcher* dispatcher,btCollisionObjectArray& collisionObjects)
{

	BT_PROFILE("islandUnionFindAndQuickSort");
	
	m_islandmanifold.resize(0);

	//we are going to sort the unionfind array, and store the element id in the size
	//afterwards, we clean unionfind, to make sure no-one uses it anymore
	
	getUnionFind().sortIslands();
	int numElem = getUnionFind().getNumElements();

	int endIslandIndex=1;
	int startIslandIndex;


	//update the sleeping state for bodies, if all are sleeping
	for ( startIslandIndex=0;startIslandIndex<numElem;startIslandIndex = endIslandIndex)
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
//				printf("error in island management\n");
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
//					printf("error in island management\n");
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
//					printf("error in island management\n");
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

	
	int i;
	int maxNumManifolds = dispatcher->getNumManifolds();

#define SPLIT_ISLANDS 1
#ifdef SPLIT_ISLANDS

	
#endif //SPLIT_ISLANDS

	
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
			if (colObj0->isKinematicObject() && colObj0->getActivationState() != ISLAND_SLEEPING)
			{
				colObj1->activate();
			}
			if (colObj1->isKinematicObject() && colObj1->getActivationState() != ISLAND_SLEEPING)
			{
				colObj0->activate();
			}
#ifdef SPLIT_ISLANDS
	//		//filtering for response
			if (dispatcher->needsResponse(colObj0,colObj1))
				m_islandmanifold.push_back(manifold);
#endif //SPLIT_ISLANDS
		}
	}
}



//
// todo: this is random access, it can be walked 'cache friendly'!
//
void btSimulationIslandManager::buildAndProcessIslands(btDispatcher* dispatcher,btCollisionObjectArray& collisionObjects, IslandCallback* callback)
{

	buildIslands(dispatcher,collisionObjects);

	int endIslandIndex=1;
	int startIslandIndex;
	int numElem = getUnionFind().getNumElements();

	BT_PROFILE("processIslands");

#ifndef SPLIT_ISLANDS
	btPersistentManifold** manifold = dispatcher->getInternalManifoldPointer();
	
	callback->ProcessIsland(&collisionObjects[0],collisionObjects.size(),manifold,maxNumManifolds, -1);
#else
	// Sort manifolds, based on islands
	// Sort the vector using predicate and std::sort
	//std::sort(islandmanifold.begin(), islandmanifold.end(), btPersistentManifoldSortPredicate);

	int numManifolds = int (m_islandmanifold.size());

	//we should do radix sort, it it much faster (O(n) instead of O (n log2(n))
	m_islandmanifold.quickSort(btPersistentManifoldSortPredicate());

	//now process all active islands (sets of manifolds for now)

	int startManifoldIndex = 0;
	int endManifoldIndex = 1;

	//int islandId;

	

//	printf("Start Islands\n");

	//traverse the simulation islands, and call the solver, unless all objects are sleeping/deactivated
	for ( startIslandIndex=0;startIslandIndex<numElem;startIslandIndex = endIslandIndex)
	{
		int islandId = getUnionFind().getElement(startIslandIndex).m_id;


	       bool islandSleeping = false;
                
                for (endIslandIndex = startIslandIndex;(endIslandIndex<numElem) && (getUnionFind().getElement(endIslandIndex).m_id == islandId);endIslandIndex++)
                {
                        int i = getUnionFind().getElement(endIslandIndex).m_sz;
                        btCollisionObject* colObj0 = collisionObjects[i];
						m_islandBodies.push_back(colObj0);
                        if (!colObj0->isActive())
                                islandSleeping = true;
                }
                

		//find the accompanying contact manifold for this islandId
		int numIslandManifolds = 0;
		btPersistentManifold** startManifold = 0;

		if (startManifoldIndex<numManifolds)
		{
			int curIslandId = getIslandId(m_islandmanifold[startManifoldIndex]);
			if (curIslandId == islandId)
			{
				startManifold = &m_islandmanifold[startManifoldIndex];
			
				for (endManifoldIndex = startManifoldIndex+1;(endManifoldIndex<numManifolds) && (islandId == getIslandId(m_islandmanifold[endManifoldIndex]));endManifoldIndex++)
				{

				}
				/// Process the actual simulation, only if not sleeping/deactivated
				numIslandManifolds = endManifoldIndex-startManifoldIndex;
			}

		}

		if (!islandSleeping)
		{
			callback->ProcessIsland(&m_islandBodies[0],m_islandBodies.size(),startManifold,numIslandManifolds, islandId);
//			printf("Island callback of size:%d bodies, %d manifolds\n",islandBodies.size(),numIslandManifolds);
		}
		
		if (numIslandManifolds)
		{
			startManifoldIndex = endManifoldIndex;
		}

		m_islandBodies.resize(0);
	}
#endif //SPLIT_ISLANDS


}
