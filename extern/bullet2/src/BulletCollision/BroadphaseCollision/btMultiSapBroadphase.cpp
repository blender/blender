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

#include "btMultiSapBroadphase.h"

#include "btSimpleBroadphase.h"
#include "LinearMath/btAabbUtil2.h"

///	btSapBroadphaseArray	m_sapBroadphases;

///	btOverlappingPairCache*	m_overlappingPairs;
extern int gOverlappingPairs;

btMultiSapBroadphase::btMultiSapBroadphase(int maxProxies,btOverlappingPairCache* pairCache)
:m_overlappingPairs(pairCache),
m_ownsPairCache(false),
m_invalidPair(0)
{
	if (!m_overlappingPairs)
	{
		m_ownsPairCache = true;
		void* mem = btAlignedAlloc(sizeof(btOverlappingPairCache),16);
		m_overlappingPairs = new (mem)btOverlappingPairCache();
	}

	struct btMultiSapOverlapFilterCallback : public btOverlapFilterCallback
	{
		virtual ~btMultiSapOverlapFilterCallback()
		{}
		// return true when pairs need collision
		virtual bool	needBroadphaseCollision(btBroadphaseProxy* childProxy0,btBroadphaseProxy* childProxy1) const
		{
			btMultiSapBroadphase::btMultiSapProxy* multiSapProxy0 = (btMultiSapBroadphase::btMultiSapProxy*)childProxy0->m_multiSapParentProxy;
			btMultiSapBroadphase::btMultiSapProxy* multiSapProxy1 = (btMultiSapBroadphase::btMultiSapProxy*)childProxy1->m_multiSapParentProxy;
			
			bool collides = (multiSapProxy0->m_collisionFilterGroup & multiSapProxy1->m_collisionFilterMask) != 0;
			collides = collides && (multiSapProxy1->m_collisionFilterGroup & multiSapProxy0->m_collisionFilterMask);
	
			return collides;
		}
	};

	void* mem = btAlignedAlloc(sizeof(btMultiSapOverlapFilterCallback),16);
	m_filterCallback = new (mem)btMultiSapOverlapFilterCallback();

	m_overlappingPairs->setOverlapFilterCallback(m_filterCallback);
	mem = btAlignedAlloc(sizeof(btSimpleBroadphase),16);
	m_simpleBroadphase = new (mem) btSimpleBroadphase(maxProxies,m_overlappingPairs);
}

btMultiSapBroadphase::~btMultiSapBroadphase()
{
	if (m_ownsPairCache)
	{
		btAlignedFree(m_overlappingPairs);
	}
}

btBroadphaseProxy*	btMultiSapBroadphase::createProxy(  const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr, short int collisionFilterGroup,short int collisionFilterMask, btDispatcher* dispatcher)
{
	void* mem = btAlignedAlloc(sizeof(btMultiSapProxy),16);
	btMultiSapProxy* proxy = new (mem)btMultiSapProxy(aabbMin,  aabbMax,shapeType,userPtr, collisionFilterGroup,collisionFilterMask);
	m_multiSapProxies.push_back(proxy);

	///we don't pass the userPtr but our multisap proxy. We need to patch this, before processing an actual collision
	///this is needed to be able to calculate the aabb overlap
	btBroadphaseProxy* simpleProxy = m_simpleBroadphase->createProxy(aabbMin,aabbMax,shapeType,userPtr,collisionFilterGroup,collisionFilterMask, dispatcher);
	simpleProxy->m_multiSapParentProxy = proxy;

	mem = btAlignedAlloc(sizeof(btChildProxy),16);
	btChildProxy* childProxyRef = new btChildProxy();
	childProxyRef->m_proxy = simpleProxy;
	childProxyRef->m_childBroadphase = m_simpleBroadphase;
	proxy->m_childProxies.push_back(childProxyRef);

	///this should deal with inserting/removal into child broadphases
	setAabb(proxy,aabbMin,aabbMax,dispatcher);
	return proxy;
}

void	btMultiSapBroadphase::destroyProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher)
{
	///not yet
	btAssert(0);

}
void	btMultiSapBroadphase::setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax, btDispatcher* dispatcher)
{
	btMultiSapProxy* multiProxy = static_cast<btMultiSapProxy*>(proxy);
	multiProxy->m_aabbMin = aabbMin;
	multiProxy->m_aabbMax = aabbMax;

	for (int i=0;i<multiProxy->m_childProxies.size();i++)
	{
		btChildProxy* childProxyRef = multiProxy->m_childProxies[i];
		childProxyRef->m_childBroadphase->setAabb(childProxyRef->m_proxy,aabbMin,aabbMax,dispatcher);
	}

}
	
	///calculateOverlappingPairs is optional: incremental algorithms (sweep and prune) might do it during the set aabb
void	btMultiSapBroadphase::calculateOverlappingPairs(btDispatcher* dispatcher)
{
	m_simpleBroadphase->calculateOverlappingPairs(dispatcher);

#ifndef USE_HASH_PAIRCACHE

	btBroadphasePairArray&	overlappingPairArray = m_overlappingPairs->getOverlappingPairArray();
	
	//perform a sort, to find duplicates and to sort 'invalid' pairs to the end
	overlappingPairArray.heapSort(btBroadphasePairSortPredicate());

	overlappingPairArray.resize(overlappingPairArray.size() - m_invalidPair);
	m_invalidPair = 0;


	btBroadphasePair previousPair;
	previousPair.m_pProxy0 = 0;
	previousPair.m_pProxy1 = 0;
	previousPair.m_algorithm = 0;
	
	int i;

	for (i=0;i<overlappingPairArray.size();i++)
	{
	
		btBroadphasePair& pair = overlappingPairArray[i];

		bool isDuplicate = (pair == previousPair);

		previousPair = pair;

		bool needsRemoval = false;

		if (!isDuplicate)
		{
			bool hasOverlap = testAabbOverlap(pair.m_pProxy0,pair.m_pProxy1);

			if (hasOverlap)
			{
				needsRemoval = false;//callback->processOverlap(pair);
			} else
			{
				needsRemoval = true;
			}
		} else
		{
			//remove duplicate
			needsRemoval = true;
			//should have no algorithm
			btAssert(!pair.m_algorithm);
		}
		
		if (needsRemoval)
		{
			m_overlappingPairs->cleanOverlappingPair(pair,dispatcher);

	//		m_overlappingPairArray.swap(i,m_overlappingPairArray.size()-1);
	//		m_overlappingPairArray.pop_back();
			pair.m_pProxy0 = 0;
			pair.m_pProxy1 = 0;
			m_invalidPair++;
			gOverlappingPairs--;
		} 
		
	}

///if you don't like to skip the invalid pairs in the array, execute following code:
#define CLEAN_INVALID_PAIRS 1
#ifdef CLEAN_INVALID_PAIRS

	//perform a sort, to sort 'invalid' pairs to the end
	overlappingPairArray.heapSort(btBroadphasePairSortPredicate());

	overlappingPairArray.resize(overlappingPairArray.size() - m_invalidPair);
	m_invalidPair = 0;
#endif//CLEAN_INVALID_PAIRS

#endif //USE_HASH_PAIRCACHE

}


bool	btMultiSapBroadphase::testAabbOverlap(btBroadphaseProxy* childProxy0,btBroadphaseProxy* childProxy1)
{
		btMultiSapProxy* multiSapProxy0 = (btMultiSapProxy*)childProxy0->m_multiSapParentProxy;
		btMultiSapProxy* multiSapProxy1 = (btMultiSapProxy*)childProxy1->m_multiSapParentProxy;

		return	TestAabbAgainstAabb2(multiSapProxy0->m_aabbMin,multiSapProxy0->m_aabbMax,
			multiSapProxy1->m_aabbMin,multiSapProxy1->m_aabbMax);
		
}
