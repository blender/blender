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
#ifndef BT_MULTI_SAP_BROADPHASE
#define BT_MULTI_SAP_BROADPHASE

#include "btBroadphaseInterface.h"
#include "LinearMath/btAlignedObjectArray.h"
#include "btOverlappingPairCache.h"

class btAxisSweep3;
class btSimpleBroadphase;


typedef btAlignedObjectArray<btAxisSweep3*> btSapBroadphaseArray;

///multi SAP broadphase
///See http://www.continuousphysics.com/Bullet/phpBB2/viewtopic.php?t=328
///and http://www.continuousphysics.com/Bullet/phpBB2/viewtopic.php?t=1329
class btMultiSapBroadphase :public btBroadphaseInterface
{
	btSapBroadphaseArray	m_sapBroadphases;
	
	btSimpleBroadphase*		m_simpleBroadphase;

	btOverlappingPairCache*	m_overlappingPairs;

	bool					m_ownsPairCache;
	
	btOverlapFilterCallback*	m_filterCallback;

	int			m_invalidPair;

	struct	btChildProxy
	{
		btBroadphaseProxy*		m_proxy;
		btBroadphaseInterface*	m_childBroadphase;
	};

public:

	struct	btMultiSapProxy	: public btBroadphaseProxy
	{

		///array with all the entries that this proxy belongs to
		btAlignedObjectArray<btChildProxy*> m_childProxies;
		btVector3	m_aabbMin;
		btVector3	m_aabbMax;

		int	m_shapeType;
		void*	m_userPtr;
		short int	m_collisionFilterGroup;
		short int	m_collisionFilterMask;

		btMultiSapProxy(const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr, short int collisionFilterGroup,short int collisionFilterMask)
			:m_aabbMin(aabbMin),
			m_aabbMax(aabbMax),
			m_shapeType(shapeType),
			m_userPtr(userPtr),
			m_collisionFilterGroup(collisionFilterGroup),
			m_collisionFilterMask(collisionFilterMask)
		{

		}

		
	};

protected:

	btAlignedObjectArray<btMultiSapProxy*> m_multiSapProxies;

public:

	btMultiSapBroadphase(int maxProxies = 16384,btOverlappingPairCache* pairCache=0);

	btSapBroadphaseArray	getBroadphaseArray()
	{
		return m_sapBroadphases;
	}

	const btSapBroadphaseArray	getBroadphaseArray() const
	{
		return m_sapBroadphases;
	}

	virtual ~btMultiSapBroadphase();

	virtual btBroadphaseProxy*	createProxy(  const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr, short int collisionFilterGroup,short int collisionFilterMask, btDispatcher* dispatcher);
	virtual void	destroyProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher);
	virtual void	setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax, btDispatcher* dispatcher);
	
	///calculateOverlappingPairs is optional: incremental algorithms (sweep and prune) might do it during the set aabb
	virtual void	calculateOverlappingPairs(btDispatcher* dispatcher);

	bool	testAabbOverlap(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1);

	virtual	btOverlappingPairCache*	getOverlappingPairCache()
	{
		return m_overlappingPairs;
	}
	virtual	const btOverlappingPairCache*	getOverlappingPairCache() const
	{
		return m_overlappingPairs;
	}
};

#endif //BT_MULTI_SAP_BROADPHASE
