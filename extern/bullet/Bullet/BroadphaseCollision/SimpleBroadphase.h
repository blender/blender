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


#include "OverlappingPairCache.h"


struct SimpleBroadphaseProxy : public BroadphaseProxy
{
	SimdVector3	m_min;
	SimdVector3	m_max;
	
	SimpleBroadphaseProxy() {};

	SimpleBroadphaseProxy(const SimdPoint3& minpt,const SimdPoint3& maxpt,int shapeType,void* userPtr,short int collisionFilterGroup,short int collisionFilterMask)
	:BroadphaseProxy(userPtr,collisionFilterGroup,collisionFilterMask),
	m_min(minpt),m_max(maxpt)		
	{
	}
	

};

///SimpleBroadphase is a brute force aabb culling broadphase based on O(n^2) aabb checks
class SimpleBroadphase : public OverlappingPairCache
{

	SimpleBroadphaseProxy*	m_proxies;
	int*				m_freeProxies;
	int				m_firstFreeProxy;

	SimpleBroadphaseProxy** m_pProxies;
	int				m_numProxies;

	

	int m_maxProxies;
	
	
	inline SimpleBroadphaseProxy*	GetSimpleProxyFromProxy(BroadphaseProxy* proxy)
	{
		SimpleBroadphaseProxy* proxy0 = static_cast<SimpleBroadphaseProxy*>(proxy);
		return proxy0;
	}

	bool	AabbOverlap(SimpleBroadphaseProxy* proxy0,SimpleBroadphaseProxy* proxy1);

	void	validate();

protected:


	virtual void	RefreshOverlappingPairs();
public:
	SimpleBroadphase(int maxProxies=4096,int maxOverlap=8192);
	virtual ~SimpleBroadphase();


	virtual BroadphaseProxy*	CreateProxy(  const SimdVector3& min,  const SimdVector3& max,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask);


	virtual void	DestroyProxy(BroadphaseProxy* proxy);
	virtual void	SetAabb(BroadphaseProxy* proxy,const SimdVector3& aabbMin,const SimdVector3& aabbMax);
		
	
	



};



#endif //SIMPLE_BROADPHASE_H

