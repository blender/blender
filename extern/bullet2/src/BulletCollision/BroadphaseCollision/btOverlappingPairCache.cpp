
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



#include "btOverlappingPairCache.h"

#include "btDispatcher.h"
#include "btCollisionAlgorithm.h"

int	gOverlappingPairs = 0;

btOverlappingPairCache::btOverlappingPairCache():
m_blockedForChanges(false),
m_overlapFilterCallback(0)
//m_NumOverlapBroadphasePair(0)
{
}


btOverlappingPairCache::~btOverlappingPairCache()
{
	//todo/test: show we erase/delete data, or is it automatic
}


void	btOverlappingPairCache::removeOverlappingPair(btBroadphasePair& findPair)
{
	
	int findIndex = m_overlappingPairArray.findLinearSearch(findPair);
	if (findIndex < m_overlappingPairArray.size())
	{
		gOverlappingPairs--;
		btBroadphasePair& pair = m_overlappingPairArray[findIndex];
		cleanOverlappingPair(pair);
		
		m_overlappingPairArray.swap(findIndex,m_overlappingPairArray.size()-1);
		m_overlappingPairArray.pop_back();
	}
}


void	btOverlappingPairCache::cleanOverlappingPair(btBroadphasePair& pair)
{
	if (pair.m_algorithm)
	{
		{
			delete pair.m_algorithm;;
			pair.m_algorithm=0;
		}
	}
}





void	btOverlappingPairCache::addOverlappingPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1)
{
	//don't add overlap with own
	assert(proxy0 != proxy1);

	if (!needsBroadphaseCollision(proxy0,proxy1))
		return;


	btBroadphasePair pair(*proxy0,*proxy1);
	
	m_overlappingPairArray.push_back(pair);
	gOverlappingPairs++;
	
}

///this findPair becomes really slow. Either sort the list to speedup the query, or
///use a different solution. It is mainly used for Removing overlapping pairs. Removal could be delayed.
///we could keep a linked list in each proxy, and store pair in one of the proxies (with lowest memory address)
///Also we can use a 2D bitmap, which can be useful for a future GPU implementation
 btBroadphasePair*	btOverlappingPairCache::findPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1)
{
	if (!needsBroadphaseCollision(proxy0,proxy1))
		return 0;

	btBroadphasePair tmpPair(*proxy0,*proxy1);
	int findIndex = m_overlappingPairArray.findLinearSearch(tmpPair);

	if (findIndex < m_overlappingPairArray.size())
	{
		//assert(it != m_overlappingPairSet.end());
		 btBroadphasePair* pair = &m_overlappingPairArray[findIndex];
		return pair;
	}
	return 0;
}





void	btOverlappingPairCache::cleanProxyFromPairs(btBroadphaseProxy* proxy)
{

	class	CleanPairCallback : public btOverlapCallback
	{
		btBroadphaseProxy* m_cleanProxy;
		btOverlappingPairCache*	m_pairCache;

	public:
		CleanPairCallback(btBroadphaseProxy* cleanProxy,btOverlappingPairCache* pairCache)
			:m_cleanProxy(cleanProxy),
			m_pairCache(pairCache)
		{
		}
		virtual	bool	processOverlap(btBroadphasePair& pair)
		{
			if ((pair.m_pProxy0 == m_cleanProxy) ||
				(pair.m_pProxy1 == m_cleanProxy))
			{
				m_pairCache->cleanOverlappingPair(pair);
			}
			return false;
		}
		
	};

	CleanPairCallback cleanPairs(proxy,this);

	processAllOverlappingPairs(&cleanPairs);

}



void	btOverlappingPairCache::removeOverlappingPairsContainingProxy(btBroadphaseProxy* proxy)
{

	class	RemovePairCallback : public btOverlapCallback
	{
		btBroadphaseProxy* m_obsoleteProxy;

	public:
		RemovePairCallback(btBroadphaseProxy* obsoleteProxy)
			:m_obsoleteProxy(obsoleteProxy)
		{
		}
		virtual	bool	processOverlap(btBroadphasePair& pair)
		{
			return ((pair.m_pProxy0 == m_obsoleteProxy) ||
				(pair.m_pProxy1 == m_obsoleteProxy));
		}
		
	};


	RemovePairCallback removeCallback(proxy);

	processAllOverlappingPairs(&removeCallback);
}



void	btOverlappingPairCache::processAllOverlappingPairs(btOverlapCallback* callback)
{

	int i;

	for (i=0;i<m_overlappingPairArray.size();)
	{
	
		btBroadphasePair* pair = &m_overlappingPairArray[i];
		if (callback->processOverlap(*pair))
		{
			cleanOverlappingPair(*pair);

			m_overlappingPairArray.swap(i,m_overlappingPairArray.size()-1);
			m_overlappingPairArray.pop_back();
			gOverlappingPairs--;
		} else
		{
			i++;
		}
	}
}

