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

#ifndef COMPOUND_COLLISION_ALGORITHM_H
#define COMPOUND_COLLISION_ALGORITHM_H

#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "BroadphaseCollision/Dispatcher.h"
#include "BroadphaseCollision/BroadphaseInterface.h"

#include "NarrowPhaseCollision/PersistentManifold.h"
class Dispatcher;
#include "BroadphaseCollision/BroadphaseProxy.h"
#include <vector>


/// CompoundCollisionAlgorithm  supports collision between CompoundCollisionShapes and other collision shapes
/// Place holder, not fully implemented yet
class CompoundCollisionAlgorithm  : public CollisionAlgorithm
{
	BroadphaseProxy	m_compoundProxy;
	BroadphaseProxy	m_otherProxy;
	std::vector<BroadphaseProxy> m_childProxies;
	std::vector<CollisionAlgorithm*> m_childCollisionAlgorithms;

	Dispatcher*		m_dispatcher;
	BroadphaseProxy m_compound;

	BroadphaseProxy m_other;

	
public:

	CompoundCollisionAlgorithm( const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	virtual ~CompoundCollisionAlgorithm();

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

	float	CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

};

#endif //COMPOUND_COLLISION_ALGORITHM_H
