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
	
//	int			m_handleId;

	
	btSimpleBroadphaseProxy() {};

	btSimpleBroadphaseProxy(const btPoint3& minpt,const btPoint3& maxpt,int shapeType,void* userPtr,short int collisionFilterGroup,short int collisionFilterMask,void* multiSapProxy)
	:btBroadphaseProxy(userPtr,collisionFilterGroup,collisionFilterMask,multiSapProxy),
	m_min(minpt),m_max(maxpt)		
	{
		(void)shapeType;
	}
	
	
	SIMD_FORCE_INLINE void SetNextFree(int next) {m_nextFree = next;}
	SIMD_FORCE_INLINE int GetNextFree() const {return m_nextFree;}

	


};

///The SimpleBroadphase is just a unit-test for btAxisSweep3, bt32BitAxisSweep3, or btDbvtBroadphase, so use those classes instead.
///It is a brute force aabb culling broadphase based on O(n^2) aabb checks
class btSimpleBroadphase : public btBroadphaseInterface
{

protected:

	int		m_numHandles;						// number of active handles
	int		m_maxHandles;						// max number of handles
	
	btSimpleBroadphaseProxy* m_pHandles;						// handles pool

	void* m_pHandlesRawPtr;
	int		m_firstFreeHandle;		// free handles list
	
	int allocHandle()
	{
		btAssert(m_numHandles < m_maxHandles);
		int freeHandle = m_firstFreeHandle;
		m_firstFreeHandle = m_pHandles[freeHandle].GetNextFree();
		m_numHandles++;
		return freeHandle;
	}

	void freeHandle(btSimpleBroadphaseProxy* proxy)
	{
		int handle = int(proxy-m_pHandles);
		btAssert(handle >= 0 && handle < m_maxHandles);

		proxy->SetNextFree(m_firstFreeHandle);
		m_firstFreeHandle = handle;

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


	virtual btBroadphaseProxy*	createProxy(  const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask, btDispatcher* dispatcher,void* multiSapProxy);

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


	///getAabb returns the axis aligned bounding box in the 'global' coordinate frame
	///will add some transform later
	virtual void getBroadphaseAabb(btVector3& aabbMin,btVector3& aabbMax) const
	{
		aabbMin.setValue(-1e30f,-1e30f,-1e30f);
		aabbMax.setValue(1e30f,1e30f,1e30f);
	}

	virtual void	printStats()
	{
//		printf("btSimpleBroadphase.h\n");
//		printf("numHandles = %d, maxHandles = %d\n",m_numHandles,m_maxHandles);
	}
};



#endif //SIMPLE_BROADPHASE_H

