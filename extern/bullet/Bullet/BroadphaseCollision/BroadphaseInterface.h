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



struct DispatcherInfo;
class Dispatcher;
struct BroadphaseProxy;
#include "SimdVector3.h"

///BroadphaseInterface for aabb-overlapping object pairs
class BroadphaseInterface
{
public:
	virtual ~BroadphaseInterface() {}

	virtual BroadphaseProxy*	CreateProxy(  const SimdVector3& min,  const SimdVector3& max,int shapeType,void* userPtr, short int collisionFilterGroup,short int collisionFilterMask) =0;
	virtual void	DestroyProxy(BroadphaseProxy* proxy)=0;
	virtual void	SetAabb(BroadphaseProxy* proxy,const SimdVector3& aabbMin,const SimdVector3& aabbMax)=0;
	virtual void	CleanProxyFromPairs(BroadphaseProxy* proxy)=0;
	

};

#endif //BROADPHASE_INTERFACE_H
