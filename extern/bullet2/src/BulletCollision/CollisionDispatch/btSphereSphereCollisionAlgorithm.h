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

#ifndef SPHERE_SPHERE_COLLISION_ALGORITHM_H
#define SPHERE_SPHERE_COLLISION_ALGORITHM_H

#include "../BroadphaseCollision/btCollisionAlgorithm.h"
#include "../BroadphaseCollision/btBroadphaseProxy.h"
#include "../CollisionDispatch/btCollisionCreateFunc.h"
class btPersistentManifold;

/// btSphereSphereCollisionAlgorithm  provides sphere-sphere collision detection.
/// Other features are frame-coherency (persistent data) and collision response.
/// Also provides the most basic sample for custom/user btCollisionAlgorithm
class btSphereSphereCollisionAlgorithm : public btCollisionAlgorithm
{
	bool	m_ownManifold;
	btPersistentManifold*	m_manifoldPtr;
	
public:
	btSphereSphereCollisionAlgorithm(btPersistentManifold* mf,const btCollisionAlgorithmConstructionInfo& ci,btCollisionObject* body0,btCollisionObject* body1);

	btSphereSphereCollisionAlgorithm(const btCollisionAlgorithmConstructionInfo& ci)
		: btCollisionAlgorithm(ci) {}

	virtual void processCollision (btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	virtual btScalar calculateTimeOfImpact(btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	
	virtual ~btSphereSphereCollisionAlgorithm();

	struct CreateFunc :public 	btCollisionAlgorithmCreateFunc
	{
		virtual	btCollisionAlgorithm* CreateCollisionAlgorithm(btCollisionAlgorithmConstructionInfo& ci, btCollisionObject* body0,btCollisionObject* body1)
		{
			return new btSphereSphereCollisionAlgorithm(0,ci,body0,body1);
		}
	};

};

#endif //SPHERE_SPHERE_COLLISION_ALGORITHM_H

