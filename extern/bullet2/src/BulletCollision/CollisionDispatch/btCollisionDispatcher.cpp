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



#include "btCollisionDispatcher.h"


#include "BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btConvexConvexAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btEmptyCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btConvexConcaveCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btCompoundCollisionAlgorithm.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/BroadphaseCollision/btOverlappingPairCache.h"

int gNumManifold = 0;

#include <stdio.h>

	
btCollisionDispatcher::btCollisionDispatcher(bool noDefaultAlgorithms):
m_count(0),
m_useIslands(true),
m_convexConvexCreateFunc(0),
m_convexConcaveCreateFunc(0),
m_swappedConvexConcaveCreateFunc(0),
m_compoundCreateFunc(0),
m_swappedCompoundCreateFunc(0),
m_emptyCreateFunc(0)
{
	(void)noDefaultAlgorithms;
	int i;

	setNearCallback(defaultNearCallback);

	m_emptyCreateFunc = new btEmptyAlgorithm::CreateFunc;
	for (i=0;i<MAX_BROADPHASE_COLLISION_TYPES;i++)
	{
		for (int j=0;j<MAX_BROADPHASE_COLLISION_TYPES;j++)
		{
			m_doubleDispatch[i][j] = m_emptyCreateFunc;
		}
	}
}
//if you want to not link with the default collision algorithms, you can
//define BT_EXCLUDE_DEFAULT_COLLISIONALGORITHM_REGISTRATION 
//in your Bullet library build system
#ifndef BT_EXCLUDE_DEFAULT_COLLISIONALGORITHM_REGISTRATION

btCollisionDispatcher::btCollisionDispatcher (): 
	m_count(0),
	m_useIslands(true)
{
	int i;

	setNearCallback(defaultNearCallback);
	
	//default CreationFunctions, filling the m_doubleDispatch table
	m_convexConvexCreateFunc = new btConvexConvexAlgorithm::CreateFunc;
	m_convexConcaveCreateFunc = new btConvexConcaveCollisionAlgorithm::CreateFunc;
	m_swappedConvexConcaveCreateFunc = new btConvexConcaveCollisionAlgorithm::SwappedCreateFunc;
	m_compoundCreateFunc = new btCompoundCollisionAlgorithm::CreateFunc;
	m_swappedCompoundCreateFunc = new btCompoundCollisionAlgorithm::SwappedCreateFunc;
	m_emptyCreateFunc = new btEmptyAlgorithm::CreateFunc;

	for (i=0;i<MAX_BROADPHASE_COLLISION_TYPES;i++)
	{
		for (int j=0;j<MAX_BROADPHASE_COLLISION_TYPES;j++)
		{
			m_doubleDispatch[i][j] = internalFindCreateFunc(i,j);
			assert(m_doubleDispatch[i][j]);
		}
	}
	
	
};

#endif //BT_EXCLUDE_DEFAULT_COLLISIONALGORITHM_REGISTRATION


void btCollisionDispatcher::registerCollisionCreateFunc(int proxyType0, int proxyType1, btCollisionAlgorithmCreateFunc *createFunc)
{
	m_doubleDispatch[proxyType0][proxyType1] = createFunc;
}

btCollisionDispatcher::~btCollisionDispatcher()
{
	delete m_convexConvexCreateFunc;
	delete m_convexConcaveCreateFunc;
	delete m_swappedConvexConcaveCreateFunc;
	delete m_compoundCreateFunc;
	delete m_swappedCompoundCreateFunc;
	delete m_emptyCreateFunc;
}

btPersistentManifold*	btCollisionDispatcher::getNewManifold(void* b0,void* b1) 
{ 
	gNumManifold++;
	
	//btAssert(gNumManifold < 65535);
	

	btCollisionObject* body0 = (btCollisionObject*)b0;
	btCollisionObject* body1 = (btCollisionObject*)b1;
	
	btPersistentManifold* manifold = new btPersistentManifold (body0,body1);
	m_manifoldsPtr.push_back(manifold);

	return manifold;
}

void btCollisionDispatcher::clearManifold(btPersistentManifold* manifold)
{
	manifold->clearManifold();
}

	
void btCollisionDispatcher::releaseManifold(btPersistentManifold* manifold)
{
	
	gNumManifold--;

	//printf("releaseManifold: gNumManifold %d\n",gNumManifold);
	clearManifold(manifold);

	///todo: this can be improved a lot, linear search might be slow part!
	int findIndex = m_manifoldsPtr.findLinearSearch(manifold);
	if (findIndex < m_manifoldsPtr.size())
	{
		m_manifoldsPtr.swap(findIndex,m_manifoldsPtr.size()-1);
		m_manifoldsPtr.pop_back();
		delete manifold;
	}
	
}

	

btCollisionAlgorithm* btCollisionDispatcher::findAlgorithm(btCollisionObject* body0,btCollisionObject* body1,btPersistentManifold* sharedManifold)
{

#ifdef USE_DISPATCH_REGISTRY_ARRAY
	
	btCollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = this;
	ci.m_manifold = sharedManifold;
	btCollisionAlgorithm* algo = m_doubleDispatch[body0->getCollisionShape()->getShapeType()][body1->getCollisionShape()->getShapeType()]
	->CreateCollisionAlgorithm(ci,body0,body1);
#else
	btCollisionAlgorithm* algo = internalFindAlgorithm(body0,body1);
#endif //USE_DISPATCH_REGISTRY_ARRAY
	return algo;
}


#ifndef BT_EXCLUDE_DEFAULT_COLLISIONALGORITHM_REGISTRATION

btCollisionAlgorithmCreateFunc* btCollisionDispatcher::internalFindCreateFunc(int proxyType0,int proxyType1)
{
	
	if (btBroadphaseProxy::isConvex(proxyType0) && btBroadphaseProxy::isConvex(proxyType1))
	{
		return m_convexConvexCreateFunc;
	}

	if (btBroadphaseProxy::isConvex(proxyType0) && btBroadphaseProxy::isConcave(proxyType1))
	{
		return m_convexConcaveCreateFunc;
	}

	if (btBroadphaseProxy::isConvex(proxyType1) && btBroadphaseProxy::isConcave(proxyType0))
	{
		return m_swappedConvexConcaveCreateFunc;
	}

	if (btBroadphaseProxy::isCompound(proxyType0))
	{
		return m_compoundCreateFunc;
	} else
	{
		if (btBroadphaseProxy::isCompound(proxyType1))
		{
			return m_swappedCompoundCreateFunc;
		}
	}

	//failed to find an algorithm
	return m_emptyCreateFunc;
}

#endif //BT_EXCLUDE_DEFAULT_COLLISIONALGORITHM_REGISTRATION


#ifndef USE_DISPATCH_REGISTRY_ARRAY

btCollisionAlgorithm* btCollisionDispatcher::internalFindAlgorithm(btCollisionObject* body0,btCollisionObject* body1,btPersistentManifold* sharedManifold)
{
	m_count++;
	
	btCollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = this;
	
	if (body0->getCollisionShape()->isConvex() && body1->getCollisionShape()->isConvex() )
	{
		return new btConvexConvexAlgorithm(sharedManifold,ci,body0,body1);
	}

	if (body0->getCollisionShape()->isConvex() && body1->getCollisionShape()->isConcave())
	{
		return new btConvexConcaveCollisionAlgorithm(ci,body0,body1,false);
	}

	if (body1->getCollisionShape()->isConvex() && body0->getCollisionShape()->isConcave())
	{
		return new btConvexConcaveCollisionAlgorithm(ci,body0,body1,true);
	}

	if (body0->getCollisionShape()->isCompound())
	{
		return new btCompoundCollisionAlgorithm(ci,body0,body1,false);
	} else
	{
		if (body1->getCollisionShape()->isCompound())
		{
			return new btCompoundCollisionAlgorithm(ci,body0,body1,true);
		}
	}

	//failed to find an algorithm
	return new btEmptyAlgorithm(ci);
	
}
#endif //USE_DISPATCH_REGISTRY_ARRAY

bool	btCollisionDispatcher::needsResponse(btCollisionObject* body0,btCollisionObject* body1)
{
	//here you can do filtering
	bool hasResponse = 
		(body0->hasContactResponse() && body1->hasContactResponse());
	//no response between two static/kinematic bodies:
	hasResponse = hasResponse &&
		((!body0->isStaticOrKinematicObject()) ||(! body1->isStaticOrKinematicObject()));
	return hasResponse;
}

bool	btCollisionDispatcher::needsCollision(btCollisionObject* body0,btCollisionObject* body1)
{
	assert(body0);
	assert(body1);

	bool needsCollision = true;

	//broadphase filtering already deals with this
	if ((body0->isStaticObject() || body0->isKinematicObject()) &&
		(body1->isStaticObject() || body1->isKinematicObject()))
	{
		printf("warning btCollisionDispatcher::needsCollision: static-static collision!\n");
	}
		
	if ((!body0->isActive()) && (!body1->isActive()))
		needsCollision = false;
	
	return needsCollision ;

}



///interface for iterating all overlapping collision pairs, no matter how those pairs are stored (array, set, map etc)
///this is useful for the collision dispatcher.
class btCollisionPairCallback : public btOverlapCallback
{
	btDispatcherInfo& m_dispatchInfo;
	btCollisionDispatcher*	m_dispatcher;

public:

	btCollisionPairCallback(btDispatcherInfo& dispatchInfo,btCollisionDispatcher*	dispatcher)
	:m_dispatchInfo(dispatchInfo),
	m_dispatcher(dispatcher)
	{
	}

	btCollisionPairCallback& operator=(btCollisionPairCallback& other)
	{
		m_dispatchInfo = other.m_dispatchInfo;
		m_dispatcher = other.m_dispatcher;
		return *this;
	}

	virtual ~btCollisionPairCallback() {}


	virtual bool	processOverlap(btBroadphasePair& pair)
	{
		(*m_dispatcher->getNearCallback())(pair,*m_dispatcher,m_dispatchInfo);

		return false;
	}
};


void	btCollisionDispatcher::dispatchAllCollisionPairs(btOverlappingPairCache* pairCache,btDispatcherInfo& dispatchInfo)
{
	//m_blockedForChanges = true;

	btCollisionPairCallback	collisionCallback(dispatchInfo,this);

	pairCache->processAllOverlappingPairs(&collisionCallback);

	//m_blockedForChanges = false;

}




//by default, Bullet will use this near callback
void btCollisionDispatcher::defaultNearCallback(btBroadphasePair& collisionPair, btCollisionDispatcher& dispatcher, btDispatcherInfo& dispatchInfo)
{
		btCollisionObject* colObj0 = (btCollisionObject*)collisionPair.m_pProxy0->m_clientObject;
		btCollisionObject* colObj1 = (btCollisionObject*)collisionPair.m_pProxy1->m_clientObject;

		if (dispatcher.needsCollision(colObj0,colObj1))
		{
			//dispatcher will keep algorithms persistent in the collision pair
			if (!collisionPair.m_algorithm)
			{
				collisionPair.m_algorithm = dispatcher.findAlgorithm(colObj0,colObj1);
			}

			if (collisionPair.m_algorithm)
			{
				btManifoldResult contactPointResult(colObj0,colObj1);
				
				if (dispatchInfo.m_dispatchFunc == 		btDispatcherInfo::DISPATCH_DISCRETE)
				{
					//discrete collision detection query
					collisionPair.m_algorithm->processCollision(colObj0,colObj1,dispatchInfo,&contactPointResult);
				} else
				{
					//continuous collision detection query, time of impact (toi)
					btScalar toi = collisionPair.m_algorithm->calculateTimeOfImpact(colObj0,colObj1,dispatchInfo,&contactPointResult);
					if (dispatchInfo.m_timeOfImpact > toi)
						dispatchInfo.m_timeOfImpact = toi;

				}
			}
		}

}
