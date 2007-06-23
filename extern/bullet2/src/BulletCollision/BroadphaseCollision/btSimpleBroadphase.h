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
	
	btSimpleBroadphaseProxy() {};

	btSimpleBroadphaseProxy(const btPoint3& minpt,const btPoint3& maxpt,int shapeType,void* userPtr,short int collisionFilterGroup,short int collisionFilterMask)
	:btBroadphaseProxy(userPtr,collisionFilterGroup,collisionFilterMask),
	m_min(minpt),m_max(maxpt)		
	{
		(void)shapeType;
	}
	

};

///SimpleBroadphase is a brute force aabb culling broadphase based on O(n^2) aabb checks
class btSimpleBroadphase : public btOverlappingPairCache
{

	btSimpleBroadphaseProxy*	m_proxies;
	int*				m_freeProxies;
	int				m_firstFreeProxy;

	btSimpleBroadphaseProxy** m_pProxies;
	int				m_numProxies;

	

	int m_maxProxies;
	
	
	inline btSimpleBroadphaseProxy*	getSimpleProxyFromProxy(btBroadphaseProxy* proxy)
	{
		btSimpleBroadphaseProxy* proxy0 = static_cast<btSimpleBroadphaseProxy*>(proxy);
		return proxy0;
	}


	void	validate();

protected:


	virtual void	refreshOverlappingPairs();
public:
	btSimpleBroadphase(int maxProxies=16384);
	virtual ~btSimpleBroadphase();


		static bool	aabbOverlap(btSimpleBroadphaseProxy* proxy0,btSimpleBroadphaseProxy* proxy1);


	virtual btBroadphaseProxy*	createProxy(  const btVector3& min,  const btVector3& max,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask);


	virtual void	destroyProxy(btBroadphaseProxy* proxy);
	virtual void	setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax);
		
	
	



};



#endif //SIMPLE_BROADPHASE_H

