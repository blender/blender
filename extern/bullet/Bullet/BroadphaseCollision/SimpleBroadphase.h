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
#ifndef SIMPLE_BROADPHASE_H
#define SIMPLE_BROADPHASE_H

#define SIMPLE_MAX_PROXIES 8192
#define SIMPLE_MAX_OVERLAP 4096

#include "BroadphaseInterface.h"
#include "BroadphaseProxy.h"
#include "SimdPoint3.h"

struct SimpleBroadphaseProxy : public BroadphaseProxy
{
	SimdVector3	m_min;
	SimdVector3	m_max;
	
	SimpleBroadphaseProxy() {};

	SimpleBroadphaseProxy(void* object,int type,const SimdPoint3& minpt,const SimdPoint3& maxpt)
	:BroadphaseProxy(object,type),
	m_min(minpt),m_max(maxpt)		
	{
	}
	

};

///SimpleBroadphase is a brute force aabb culling broadphase based on O(n^2) aabb checks
class SimpleBroadphase : public BroadphaseInterface
{

	SimpleBroadphaseProxy	m_proxies[SIMPLE_MAX_PROXIES];
	int				m_freeProxies[SIMPLE_MAX_PROXIES];
	int				m_firstFreeProxy;

	BroadphaseProxy* m_pProxies[SIMPLE_MAX_PROXIES];
	int				m_numProxies;

	//during the dispatch, check that user doesn't destroy/create proxy
	bool		m_blockedForChanges;

	BroadphasePair	m_OverlappingPairs[SIMPLE_MAX_OVERLAP];
	int	m_NumOverlapBroadphasePair;

	
	SimpleBroadphaseProxy*	GetSimpleProxyFromProxy(BroadphaseProxy* proxy);

	bool	AabbOverlap(SimpleBroadphaseProxy* proxy0,SimpleBroadphaseProxy* proxy1);
	void	RemoveOverlappingPair(BroadphasePair& pair);
	void	CleanOverlappingPair(BroadphasePair& pair);
	void	AddOverlappingPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);
	bool	FindPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);
	void	RefreshOverlappingPairs();
public:
	SimpleBroadphase();
	virtual ~SimpleBroadphase();

	virtual BroadphaseProxy*	CreateProxy(  void *object,int type, const SimdVector3& min,  const SimdVector3& max) ;
	virtual void	DestroyProxy(BroadphaseProxy* proxy);
	virtual void	SetAabb(BroadphaseProxy* proxy,const SimdVector3& aabbMin,const SimdVector3& aabbMax);
	virtual void	CleanProxyFromPairs(BroadphaseProxy* proxy);
	virtual void	DispatchAllCollisionPairs(Dispatcher&	dispatcher,DispatcherInfo& dispatchInfo);

};



#endif //SIMPLE_BROADPHASE_H

