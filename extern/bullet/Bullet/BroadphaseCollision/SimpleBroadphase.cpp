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

#include "SimpleBroadphase.h"
#include "BroadphaseCollision/Dispatcher.h"
#include "BroadphaseCollision/CollisionAlgorithm.h"

#include "SimdVector3.h"
#include "SimdTransform.h"
#include "SimdMatrix3x3.h"
#include <vector>


void	SimpleBroadphase::validate()
{
	for (int i=0;i<m_numProxies;i++)
	{
		for (int j=i+1;j<m_numProxies;j++)
		{
			assert(m_pProxies[i] != m_pProxies[j]);
		}
	}
	
}

SimpleBroadphase::SimpleBroadphase(int maxProxies,int maxOverlap)
	:m_firstFreeProxy(0),
	m_numProxies(0),
	m_blockedForChanges(false),
	m_NumOverlapBroadphasePair(0),
	m_maxProxies(maxProxies),
	m_maxOverlap(maxOverlap)
{

	m_proxies = new SimpleBroadphaseProxy[maxProxies];
	m_freeProxies = new int[maxProxies];
	m_pProxies = new SimpleBroadphaseProxy*[maxProxies];
	m_OverlappingPairs = new BroadphasePair[maxOverlap];


	int i;
	for (i=0;i<m_maxProxies;i++)
	{
		m_freeProxies[i] = i;
	}
}

SimpleBroadphase::~SimpleBroadphase()
{
	delete[] m_proxies;
	delete []m_freeProxies;
	delete [] m_pProxies;
	delete [] m_OverlappingPairs;

	/*int i;
	for (i=m_numProxies-1;i>=0;i--)
	{
		BP_Proxy* proxy = m_pProxies[i]; 
		destroyProxy(proxy);
	}
	*/
}


BroadphaseProxy*	SimpleBroadphase::CreateProxy(  const SimdVector3& min,  const SimdVector3& max,int shapeType,void* userPtr)
{
	if (m_numProxies >= m_maxProxies)
	{
		assert(0);
		return 0; //should never happen, but don't let the game crash ;-)
	}
	assert(min[0]<= max[0] && min[1]<= max[1] && min[2]<= max[2]);

	int freeIndex= m_freeProxies[m_firstFreeProxy];
	SimpleBroadphaseProxy* proxy = new (&m_proxies[freeIndex])SimpleBroadphaseProxy(min,max,shapeType,userPtr);
	m_firstFreeProxy++;

	SimpleBroadphaseProxy* proxy1 = &m_proxies[0];
		
	int index = proxy - proxy1;
	assert(index == freeIndex);

	m_pProxies[m_numProxies] = proxy;
	m_numProxies++;
	//validate();

	return proxy;
}

void	SimpleBroadphase::RemoveOverlappingPairsContainingProxy(BroadphaseProxy* proxy)
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

void	SimpleBroadphase::DestroyProxy(BroadphaseProxy* proxyOrg)
{
		
		int i;
		
		SimpleBroadphaseProxy* proxy0 = static_cast<SimpleBroadphaseProxy*>(proxyOrg);
		SimpleBroadphaseProxy* proxy1 = &m_proxies[0];
	
		int index = proxy0 - proxy1;
		assert (index < m_maxProxies);
		m_freeProxies[--m_firstFreeProxy] = index;

		RemoveOverlappingPairsContainingProxy(proxyOrg);

		
		for (i=0;i<m_numProxies;i++)
		{
			if (m_pProxies[i] == proxyOrg)
			{
				m_pProxies[i] = m_pProxies[m_numProxies-1];
				break;
			}
		}
		m_numProxies--;
		//validate();
		
}

void	SimpleBroadphase::SetAabb(BroadphaseProxy* proxy,const SimdVector3& aabbMin,const SimdVector3& aabbMax)
{
	SimpleBroadphaseProxy* sbp = GetSimpleProxyFromProxy(proxy);
	sbp->m_min = aabbMin;
	sbp->m_max = aabbMax;
}

void	SimpleBroadphase::CleanOverlappingPair(BroadphasePair& pair)
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


void	SimpleBroadphase::CleanProxyFromPairs(BroadphaseProxy* proxy)
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

void	SimpleBroadphase::AddOverlappingPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
{
	//don't add overlap with own
	assert(proxy0 != proxy1);

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
	
BroadphasePair*	SimpleBroadphase::FindPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
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
void	SimpleBroadphase::RemoveOverlappingPair(BroadphasePair& pair)
{
    CleanOverlappingPair(pair);
	int	index = &pair - &m_OverlappingPairs[0];
	//remove efficiently, swap with the last
	m_OverlappingPairs[index] = m_OverlappingPairs[m_NumOverlapBroadphasePair-1];
	m_NumOverlapBroadphasePair--;
}

bool	SimpleBroadphase::AabbOverlap(SimpleBroadphaseProxy* proxy0,SimpleBroadphaseProxy* proxy1)
{
	return proxy0->m_min[0] <= proxy1->m_max[0] && proxy1->m_min[0] <= proxy0->m_max[0] && 
		   proxy0->m_min[1] <= proxy1->m_max[1] && proxy1->m_min[1] <= proxy0->m_max[1] &&
		   proxy0->m_min[2] <= proxy1->m_max[2] && proxy1->m_min[2] <= proxy0->m_max[2];

}
void	SimpleBroadphase::RefreshOverlappingPairs()
{
	//first check for new overlapping pairs
	int i,j;

	for (i=0;i<m_numProxies;i++)
	{
		BroadphaseProxy* proxy0 = m_pProxies[i];
		for (j=i+1;j<m_numProxies;j++)
		{
			BroadphaseProxy* proxy1 = m_pProxies[j];
			SimpleBroadphaseProxy* p0 = GetSimpleProxyFromProxy(proxy0);
			SimpleBroadphaseProxy* p1 = GetSimpleProxyFromProxy(proxy1);

			if (AabbOverlap(p0,p1))
			{
				if ( !FindPair(proxy0,proxy1))
				{
					AddOverlappingPair(proxy0,proxy1);
				}
			}

		}
	}

	//then remove non-overlapping ones
	for (i=0;i<m_NumOverlapBroadphasePair;i++)
	{
		BroadphasePair& pair = m_OverlappingPairs[i];
		SimpleBroadphaseProxy* proxy0 = GetSimpleProxyFromProxy(pair.m_pProxy0);
		SimpleBroadphaseProxy* proxy1 = GetSimpleProxyFromProxy(pair.m_pProxy1);
		if (!AabbOverlap(proxy0,proxy1))
		{
            RemoveOverlappingPair(pair);
		}
	}

	

}

void	SimpleBroadphase::DispatchAllCollisionPairs(Dispatcher&	dispatcher,DispatcherInfo& dispatchInfo)
{
	m_blockedForChanges = true;

	int i;
	
	int dispatcherId = dispatcher.GetUniqueId();

	RefreshOverlappingPairs();

	for (i=0;i<m_NumOverlapBroadphasePair;i++)
	{
		
		BroadphasePair& pair = m_OverlappingPairs[i];
		
		if (dispatcherId>= 0)
		{
			//dispatcher will keep algorithms persistent in the collision pair
			if (!pair.m_algorithms[dispatcherId])
			{
				pair.m_algorithms[dispatcherId] = dispatcher.FindAlgorithm(
					*pair.m_pProxy0,
					*pair.m_pProxy1);
			}

			if (pair.m_algorithms[dispatcherId])
			{
				if (dispatchInfo.m_dispatchFunc == 		DispatcherInfo::DISPATCH_DISCRETE)
				{
					pair.m_algorithms[dispatcherId]->ProcessCollision(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
				} else
				{
					float toi = pair.m_algorithms[dispatcherId]->CalculateTimeOfImpact(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
					if (dispatchInfo.m_timeOfImpact > toi)
						dispatchInfo.m_timeOfImpact = toi;

				}
			}
		} else
		{
			//non-persistent algorithm dispatcher
				CollisionAlgorithm* algo = dispatcher.FindAlgorithm(
					*pair.m_pProxy0,
					*pair.m_pProxy1);

				if (algo)
				{
					if (dispatchInfo.m_dispatchFunc == 		DispatcherInfo::DISPATCH_DISCRETE)
					{
						algo->ProcessCollision(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
					} else
					{
						float toi = algo->CalculateTimeOfImpact(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
						if (dispatchInfo.m_timeOfImpact > toi)
							dispatchInfo.m_timeOfImpact = toi;
					}
				}
		}

	}

	m_blockedForChanges = false;

}


