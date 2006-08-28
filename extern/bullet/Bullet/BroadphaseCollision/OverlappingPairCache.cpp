
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



#include "OverlappingPairCache.h"

#include "Dispatcher.h"
#include "CollisionAlgorithm.h"


OverlappingPairCache::OverlappingPairCache(int maxOverlap):
m_blockedForChanges(false),
m_NumOverlapBroadphasePair(0),
m_maxOverlap(maxOverlap)
{
	m_OverlappingPairs = new BroadphasePair[maxOverlap];
}


OverlappingPairCache::~OverlappingPairCache()
{
	delete [] m_OverlappingPairs;
}


void	OverlappingPairCache::RemoveOverlappingPair(BroadphasePair& pair)
{
	CleanOverlappingPair(pair);
	int	index = &pair - &m_OverlappingPairs[0];
	//remove efficiently, swap with the last
	m_OverlappingPairs[index] = m_OverlappingPairs[m_NumOverlapBroadphasePair-1];
	m_NumOverlapBroadphasePair--;
}


void	OverlappingPairCache::CleanOverlappingPair(BroadphasePair& pair)
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





void	OverlappingPairCache::AddOverlappingPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
{
	//don't add overlap with own
	assert(proxy0 != proxy1);

	if (!NeedsCollision(proxy0,proxy1))
		return;


	BroadphasePair pair(*proxy0,*proxy1);
	m_OverlappingPairs[m_NumOverlapBroadphasePair] = pair;

	int i;
	for (i=0;i<SIMPLE_MAX_ALGORITHMS;i++)
	{
		assert(!m_OverlappingPairs[m_NumOverlapBroadphasePair].m_algorithms[i]);
		m_OverlappingPairs[m_NumOverlapBroadphasePair].m_algorithms[i] = 0;
	}

	if (m_NumOverlapBroadphasePair >= m_maxOverlap)
	{
		//printf("Error: too many overlapping objects: m_NumOverlapBroadphasePair: %d\n",m_NumOverlapBroadphasePair);
#ifdef DEBUG
		assert(0);
#endif 
	} else
	{
		m_NumOverlapBroadphasePair++;
	}


}


BroadphasePair*	OverlappingPairCache::FindPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
{
	BroadphasePair* foundPair = 0;

	int i;
	for (i=m_NumOverlapBroadphasePair-1;i>=0;i--)
	{
		BroadphasePair& pair = m_OverlappingPairs[i];
		if (((pair.m_pProxy0 == proxy0) && (pair.m_pProxy1 == proxy1)) ||
			((pair.m_pProxy0 == proxy1) && (pair.m_pProxy1 == proxy0)))
		{
			foundPair = &pair;
			return foundPair;
		}
	}	

	return foundPair;
}



void	OverlappingPairCache::CleanProxyFromPairs(BroadphaseProxy* proxy)
{
	for (int i=0;i<m_NumOverlapBroadphasePair;i++)
	{
		BroadphasePair& pair = m_OverlappingPairs[i];
		if (pair.m_pProxy0 == proxy ||
			pair.m_pProxy1 == proxy)
		{
			CleanOverlappingPair(pair);
		}
	}
}

void	OverlappingPairCache::RemoveOverlappingPairsContainingProxy(BroadphaseProxy* proxy)
{
	int i;
	for ( i=m_NumOverlapBroadphasePair-1;i>=0;i--)
	{
		BroadphasePair& pair = m_OverlappingPairs[i];
		if (pair.m_pProxy0 == proxy ||
			pair.m_pProxy1 == proxy)
		{
			RemoveOverlappingPair(pair);
		}
	}
}



