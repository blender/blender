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
#include <BroadphaseCollision/Dispatcher.h>
#include <BroadphaseCollision/CollisionAlgorithm.h>

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
	:OverlappingPairCache(maxOverlap),
	m_firstFreeProxy(0),
	m_numProxies(0),
	m_maxProxies(maxProxies)
{

	m_proxies = new SimpleBroadphaseProxy[maxProxies];
	m_freeProxies = new int[maxProxies];
	m_pProxies = new SimpleBroadphaseProxy*[maxProxies];
	

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

	/*int i;
	for (i=m_numProxies-1;i>=0;i--)
	{
		BP_Proxy* proxy = m_pProxies[i]; 
		destroyProxy(proxy);
	}
	*/
}


BroadphaseProxy*	SimpleBroadphase::CreateProxy(  const SimdVector3& min,  const SimdVector3& max,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask)
{
	if (m_numProxies >= m_maxProxies)
	{
		assert(0);
		return 0; //should never happen, but don't let the game crash ;-)
	}
	assert(min[0]<= max[0] && min[1]<= max[1] && min[2]<= max[2]);

	int freeIndex= m_freeProxies[m_firstFreeProxy];
	SimpleBroadphaseProxy* proxy = new (&m_proxies[freeIndex])SimpleBroadphaseProxy(min,max,shapeType,userPtr,collisionFilterGroup,collisionFilterMask);
	m_firstFreeProxy++;

	SimpleBroadphaseProxy* proxy1 = &m_proxies[0];
		
	int index = proxy - proxy1;
	assert(index == freeIndex);

	m_pProxies[m_numProxies] = proxy;
	m_numProxies++;
	//validate();

	return proxy;
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
	for (i=0;i<GetNumOverlappingPairs();i++)
	{
		BroadphasePair& pair = GetOverlappingPair(i);

		SimpleBroadphaseProxy* proxy0 = GetSimpleProxyFromProxy(pair.m_pProxy0);
		SimpleBroadphaseProxy* proxy1 = GetSimpleProxyFromProxy(pair.m_pProxy1);
		if (!AabbOverlap(proxy0,proxy1))
		{
            RemoveOverlappingPair(pair);
		}
	}

	

}


