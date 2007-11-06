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

#ifndef SIMPLE_BROADPHASE_H
#define SIMPLE_BROADPHASE_H


#include "btOverlappingPairCache.h"


struct btSimpleBroadphaseProxy : public btBroadphaseProxy
{
	btVector3	m_min;
	btVector3	m_max;
	int			m_nextFree;
	int			m_nextAllocated;
//	int			m_handleId;

	
	btSimpleBroadphaseProxy() {};

	btSimpleBroadphaseProxy(const btPoint3& minpt,const btPoint3& maxpt,int shapeType,void* userPtr,short int collisionFilterGroup,short int collisionFilterMask)
	:btBroadphaseProxy(userPtr,collisionFilterGroup,collisionFilterMask),
	m_min(minpt),m_max(maxpt)		
	{
		(void)shapeType;
	}
	
	
	SIMD_FORCE_INLINE void SetNextFree(int next) {m_nextFree = next;}
	SIMD_FORCE_INLINE int GetNextFree() const {return m_nextFree;}

	SIMD_FORCE_INLINE void SetNextAllocated(int next) {m_nextAllocated = next;}
	SIMD_FORCE_INLINE int GetNextAllocated() const {return m_nextAllocated;}


};

///SimpleBroadphase is a brute force aabb culling broadphase based on O(n^2) aabb checks
///btSimpleBroadphase is just a unit-test implementation to verify and test other broadphases.
///So please don't use this class, but use bt32BitAxisSweep3 or btAxisSweep3 instead!
class btSimpleBroadphase : public btBroadphaseInterface
{

protected:

	int		m_numHandles;						// number of active handles
	int		m_maxHandles;						// max number of handles
	btSimpleBroadphaseProxy* m_pHandles;						// handles pool
	int		m_firstFreeHandle;		// free handles list
	int		m_firstAllocatedHandle;

	int allocHandle()
	{

		int freeHandle = m_firstFreeHandle;
		m_firstFreeHandle = m_pHandles[freeHandle].GetNextFree();
		
		m_pHandles[freeHandle].SetNextAllocated(m_firstAllocatedHandle);
		m_firstAllocatedHandle = freeHandle;
		
		m_numHandles++;

		return freeHandle;
	}

	void freeHandle(btSimpleBroadphaseProxy* proxy)
	{
		int handle = int(proxy-m_pHandles);
		btAssert(handle >= 0 && handle < m_maxHandles);

		proxy->SetNextFree(m_firstFreeHandle);
		m_firstFreeHandle = handle;

		m_firstAllocatedHandle = proxy->GetNextAllocated();
		proxy->SetNextAllocated(-1);

		m_numHandles--;
	}


	btOverlappingPairCache*	m_pairCache;
	bool	m_ownsPairCache;

	int	m_invalidPair;

	
	
	inline btSimpleBroadphaseProxy*	getSimpleProxyFromProxy(btBroadphaseProxy* proxy)
	{
		btSimpleBroadphaseProxy* proxy0 = static_cast<btSimpleBroadphaseProxy*>(proxy);
		return proxy0;
	}


	void	validate();

protected:


	

public:
	btSimpleBroadphase(int maxProxies=16384,btOverlappingPairCache* overlappingPairCache=0);
	virtual ~btSimpleBroadphase();


		static bool	aabbOverlap(btSimpleBroadphaseProxy* proxy0,btSimpleBroadphaseProxy* proxy1);


	virtual btBroadphaseProxy*	createProxy(  const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask, btDispatcher* dispatcher);

	virtual void	calculateOverlappingPairs(btDispatcher* dispatcher);

	virtual void	destroyProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher);
	virtual void	setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax, btDispatcher* dispatcher);
		
	btOverlappingPairCache*	getOverlappingPairCache()
	{
		return m_pairCache;
	}
	const btOverlappingPairCache*	getOverlappingPairCache() const
	{
		return m_pairCache;
	}

	bool	testAabbOverlap(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1);


};



#endif //SIMPLE_BROADPHASE_H

