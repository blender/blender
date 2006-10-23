
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
m_blockedForChanges(false)
//m_NumOverlapBroadphasePair(0)
{
}


btOverlappingPairCache::~btOverlappingPairCache()
{
	//todo/test: show we erase/delete data, or is it automatic
}


void	btOverlappingPairCache::removeOverlappingPair(btBroadphasePair& findPair)
{
	
	std::set<btBroadphasePair>::iterator it = m_overlappingPairSet.find(findPair);
//	assert(it != m_overlappingPairSet.end());

	if (it != m_overlappingPairSet.end())
	{
		gOverlappingPairs--;
		btBroadphasePair* pair = (btBroadphasePair*)(&(*it));
		cleanOverlappingPair(*pair);	
		m_overlappingPairSet.erase(it);
	}
}


void	btOverlappingPairCache::cleanOverlappingPair(btBroadphasePair& pair)
{
	for (int dispatcherId=0;dispatcherId<SIMPLE_MAX_ALGORITHMS;dispatcherId++)
	{
		if (pair.m_algorithms[dispatcherId])
		{
			{
				delete pair.m_algorithms[dispatcherId];
				pair.m_algorithms[dispatcherId]=0;
			}
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
	
	m_overlappingPairSet.insert(pair);
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
	std::set<btBroadphasePair>::iterator it = m_overlappingPairSet.find(tmpPair);
	if ((it == m_overlappingPairSet.end()))
		return 0;

	//assert(it != m_overlappingPairSet.end());
	 btBroadphasePair* pair = (btBroadphasePair*)(&(*it));
	return pair;
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
	std::set<btBroadphasePair>::iterator it = m_overlappingPairSet.begin();
	for (; !(it==m_overlappingPairSet.end());)
	{
	
		btBroadphasePair* pair = (btBroadphasePair*)(&(*it));
		if (callback->processOverlap(*pair))
		{
			cleanOverlappingPair(*pair);

			std::set<btBroadphasePair>::iterator it2 = it;
			//why does next line not compile under OS X??
#ifdef MAC_OSX_FIXED_STL_SET
			it2++;
			it = m_overlappingPairSet.erase(it);
			assert(it == it2);
#else
			it++;
			m_overlappingPairSet.erase(it2);
#endif //MAC_OSX_FIXED_STL_SET

			gOverlappingPairs--;
		} else
		{
			it++;
		}
	}
}

