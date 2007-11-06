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

#ifndef		BROADPHASE_INTERFACE_H
#define 	BROADPHASE_INTERFACE_H



struct btDispatcherInfo;
class btDispatcher;
#include "btBroadphaseProxy.h"
class btOverlappingPairCache;

#include "LinearMath/btVector3.h"

///BroadphaseInterface for aabb-overlapping object pairs
class btBroadphaseInterface
{
public:
	virtual ~btBroadphaseInterface() {}

	virtual btBroadphaseProxy*	createProxy(  const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr, short int collisionFilterGroup,short int collisionFilterMask, btDispatcher* dispatcher) =0;
	virtual void	destroyProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher)=0;
	virtual void	setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax, btDispatcher* dispatcher)=0;
	
	///calculateOverlappingPairs is optional: incremental algorithms (sweep and prune) might do it during the set aabb
	virtual void	calculateOverlappingPairs(btDispatcher* dispatcher)=0;

	virtual	btOverlappingPairCache*	getOverlappingPairCache()=0;
	virtual	const btOverlappingPairCache*	getOverlappingPairCache() const =0;

};

#endif //BROADPHASE_INTERFACE_H
