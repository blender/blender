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

#include "btSimpleBroadphase.h"
#include <BulletCollision/BroadphaseCollision/btDispatcher.h>
#include <BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h>

#include "LinearMath/btVector3.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btMatrix3x3.h"
#include <new>


void	btSimpleBroadphase::validate()
{
	for (int i=0;i<m_numProxies;i++)
	{
		for (int j=i+1;j<m_numProxies;j++)
		{
			assert(m_pProxies[i] != m_pProxies[j]);
		}
	}
	
}

btSimpleBroadphase::btSimpleBroadphase(int maxProxies)
	:btOverlappingPairCache(),
	m_firstFreeProxy(0),
	m_numProxies(0),
	m_maxProxies(maxProxies)
{

	m_proxies = new btSimpleBroadphaseProxy[maxProxies];
	m_freeProxies = new int[maxProxies];
	m_pProxies = new btSimpleBroadphaseProxy*[maxProxies];
	

	int i;
	for (i=0;i<m_maxProxies;i++)
	{
		m_freeProxies[i] = i;
	}
}

btSimpleBroadphase::~btSimpleBroadphase()
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


btBroadphaseProxy*	btSimpleBroadphase::createProxy(  const btVector3& min,  const btVector3& max,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask)
{
	if (m_numProxies >= m_maxProxies)
	{
		assert(0);
		return 0; //should never happen, but don't let the game crash ;-)
	}
	assert(min[0]<= max[0] && min[1]<= max[1] && min[2]<= max[2]);

	int freeIndex= m_freeProxies[m_firstFreeProxy];
	btSimpleBroadphaseProxy* proxy = new (&m_proxies[freeIndex])btSimpleBroadphaseProxy(min,max,shapeType,userPtr,collisionFilterGroup,collisionFilterMask);
	m_firstFreeProxy++;

	btSimpleBroadphaseProxy* proxy1 = &m_proxies[0];
		
	int	index = int(proxy - proxy1);
	btAssert(index == freeIndex);

	m_pProxies[m_numProxies] = proxy;
	m_numProxies++;
	//validate();

	return proxy;
}

class	RemovingOverlapCallback : public btOverlapCallback
{
protected:
	virtual bool	processOverlap(btBroadphasePair& pair)
	{
		(void)pair;
		btAssert(0);
		return false;
	}
};

class RemovePairContainingProxy
{

	btBroadphaseProxy*	m_targetProxy;
	public:
	virtual ~RemovePairContainingProxy()
	{
	}
protected:
	virtual bool processOverlap(btBroadphasePair& pair)
	{
		btSimpleBroadphaseProxy* proxy0 = static_cast<btSimpleBroadphaseProxy*>(pair.m_pProxy0);
		btSimpleBroadphaseProxy* proxy1 = static_cast<btSimpleBroadphaseProxy*>(pair.m_pProxy1);

		return ((m_targetProxy == proxy0 || m_targetProxy == proxy1));
	};
};

void	btSimpleBroadphase::destroyProxy(btBroadphaseProxy* proxyOrg)
{
		
		int i;
		
		btSimpleBroadphaseProxy* proxy0 = static_cast<btSimpleBroadphaseProxy*>(proxyOrg);
		btSimpleBroadphaseProxy* proxy1 = &m_proxies[0];
	
		int index = int(proxy0 - proxy1);
		btAssert (index < m_maxProxies);
		m_freeProxies[--m_firstFreeProxy] = index;

		removeOverlappingPairsContainingProxy(proxyOrg);
		
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

void	btSimpleBroadphase::setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax)
{
	btSimpleBroadphaseProxy* sbp = getSimpleProxyFromProxy(proxy);
	sbp->m_min = aabbMin;
	sbp->m_max = aabbMax;
}





	



bool	btSimpleBroadphase::aabbOverlap(btSimpleBroadphaseProxy* proxy0,btSimpleBroadphaseProxy* proxy1)
{
	return proxy0->m_min[0] <= proxy1->m_max[0] && proxy1->m_min[0] <= proxy0->m_max[0] && 
		   proxy0->m_min[1] <= proxy1->m_max[1] && proxy1->m_min[1] <= proxy0->m_max[1] &&
		   proxy0->m_min[2] <= proxy1->m_max[2] && proxy1->m_min[2] <= proxy0->m_max[2];

}



//then remove non-overlapping ones
class CheckOverlapCallback : public btOverlapCallback
{
public:
	virtual bool processOverlap(btBroadphasePair& pair)
	{
		return (!btSimpleBroadphase::aabbOverlap(static_cast<btSimpleBroadphaseProxy*>(pair.m_pProxy0),static_cast<btSimpleBroadphaseProxy*>(pair.m_pProxy1)));
	}
};

void	btSimpleBroadphase::refreshOverlappingPairs()
{
	//first check for new overlapping pairs
	int i,j;

	for (i=0;i<m_numProxies;i++)
	{
		btBroadphaseProxy* proxy0 = m_pProxies[i];
		for (j=i+1;j<m_numProxies;j++)
		{
			btBroadphaseProxy* proxy1 = m_pProxies[j];
			btSimpleBroadphaseProxy* p0 = getSimpleProxyFromProxy(proxy0);
			btSimpleBroadphaseProxy* p1 = getSimpleProxyFromProxy(proxy1);

			if (aabbOverlap(p0,p1))
			{
				if ( !findPair(proxy0,proxy1))
				{
					addOverlappingPair(proxy0,proxy1);
				}
			}

		}
	}


	CheckOverlapCallback	checkOverlap;

	processAllOverlappingPairs(&checkOverlap);


}


