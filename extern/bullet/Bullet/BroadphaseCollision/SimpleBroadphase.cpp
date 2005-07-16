/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#include "SimpleBroadphase.h"
#include "BroadphaseCollision/CollisionDispatcher.h"
#include "BroadphaseCollision/CollisionAlgorithm.h"

#include "SimdVector3.h"
#include "SimdTransform.h"
#include "SimdMatrix3x3.h"
#include <vector>
#include "NarrowPhaseCollision/CollisionMargin.h"

SimpleBroadphase::SimpleBroadphase()
:  m_firstFreeProxy(0),
  m_numProxies(0),
 m_blockedForChanges(false),
 m_NumOverlapBroadphasePair(0)
{
	int i;
	for (i=0;i<SIMPLE_MAX_PROXIES;i++)
	{
		m_freeProxies[i] = i;
	}
}

SimpleBroadphase::~SimpleBroadphase()
{
	/*int i;
	for (i=m_numProxies-1;i>=0;i--)
	{
		BP_Proxy* proxy = m_pProxies[i]; 
		destroyProxy(proxy);
	}
	*/
}


BroadphaseProxy*	SimpleBroadphase::CreateProxy( void *object, int type, const SimdVector3& min,  const SimdVector3& max) 
{
	if (m_numProxies >= SIMPLE_MAX_PROXIES)
	{
		assert(0);
		return 0; //should never happen, but don't let the game crash ;-)
	}
	assert(min[0]<= max[0] && min[1]<= max[1] && min[2]<= max[2]);

	int freeIndex= m_freeProxies[m_firstFreeProxy];
	BroadphaseProxy* proxy = new (&m_proxies[freeIndex])SimpleBroadphaseProxy(object,type,min,max);
	m_firstFreeProxy++;

	m_pProxies[m_numProxies] = proxy;
	m_numProxies++;

	return proxy;
}
void	SimpleBroadphase::DestroyProxy(BroadphaseProxy* proxy)
{
		m_numProxies--;
		BroadphaseProxy* proxy1 = &m_proxies[0];
	
		int index = proxy - proxy1;
		m_freeProxies[--m_firstFreeProxy] = index;

}

SimpleBroadphaseProxy*	SimpleBroadphase::GetSimpleProxyFromProxy(BroadphaseProxy* proxy)
{
	SimpleBroadphaseProxy* proxy0 = static_cast<SimpleBroadphaseProxy*>(proxy);

	int index = proxy0 - &m_proxies[0];
	assert(index < m_numProxies);

	SimpleBroadphaseProxy* sbp = &m_proxies[index];
	return sbp;
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
	BroadphasePair pair(*proxy0,*proxy1);
	m_OverlappingPairs[m_NumOverlapBroadphasePair] = pair;

	int i;
	for (i=0;i<SIMPLE_MAX_ALGORITHMS;i++)
	{
		assert(!m_OverlappingPairs[m_NumOverlapBroadphasePair].m_algorithms[i]);
		m_OverlappingPairs[m_NumOverlapBroadphasePair].m_algorithms[i] = 0;
	}
	m_NumOverlapBroadphasePair++;
}
	
bool	SimpleBroadphase::FindPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
{
	bool found = false;
	int i;
	for (i=m_NumOverlapBroadphasePair-1;i>=0;i--)
	{
		BroadphasePair& pair = m_OverlappingPairs[i];
		if (((pair.m_pProxy0 == proxy0) && (pair.m_pProxy1 == proxy1)) ||
			((pair.m_pProxy0 == proxy1) && (pair.m_pProxy1 == proxy0)))
		{
			found = true;
			break;
		}
	}	

	return found;
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

	//BroadphasePair	m_OverlappingPairs[SIMPLE_MAX_OVERLAP];
	//int	m_NumOverlapBroadphasePair;

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
					pair.m_algorithms[dispatcherId]->ProcessCollision(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo.m_timeStep,dispatchInfo.m_stepCount,dispatchInfo.m_useContinuous);
				} else
				{
					float toi = pair.m_algorithms[dispatcherId]->CalculateTimeOfImpact(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo.m_timeStep,dispatchInfo.m_stepCount);
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
						algo->ProcessCollision(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo.m_timeStep,dispatchInfo.m_stepCount,dispatchInfo.m_useContinuous);
					} else
					{
						float toi = algo->CalculateTimeOfImpact(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo.m_timeStep,dispatchInfo.m_stepCount);
						if (dispatchInfo.m_timeOfImpact > toi)
							dispatchInfo.m_timeOfImpact = toi;
					}
				}
		}

	}

	m_blockedForChanges = false;

}


