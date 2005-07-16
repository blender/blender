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
#ifndef		BROADPHASE_INTERFACE_H
#define 	BROADPHASE_INTERFACE_H


struct	DispatcherInfo;
class Dispatcher;
struct BroadphaseProxy;
#include "SimdVector3.h"

///BroadphaseInterface for aabb-overlapping object pairs
class BroadphaseInterface
{
public:
	virtual BroadphaseProxy*	CreateProxy(  void *object,int type, const SimdVector3& min,  const SimdVector3& max) =0;
	virtual void	DestroyProxy(BroadphaseProxy* proxy)=0;
	virtual void	SetAabb(BroadphaseProxy* proxy,const SimdVector3& aabbMin,const SimdVector3& aabbMax)=0;
	virtual void	CleanProxyFromPairs(BroadphaseProxy* proxy)=0;
	virtual void	DispatchAllCollisionPairs(Dispatcher&	dispatcher,DispatcherInfo& dispatchInfo)=0;

};

#endif //BROADPHASE_INTERFACE_H
