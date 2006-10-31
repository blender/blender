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
#include <algorithm>
#include "BulletCollision/BroadphaseCollision/btOverlappingPairCache.h"

int gNumManifold = 0;

#include <stdio.h>

	
btCollisionDispatcher::btCollisionDispatcher(bool noDefaultAlgorithms)
:m_useIslands(true),
m_count(0),
m_convexConvexCreateFunc(0),
m_convexConcaveCreateFunc(0),
m_swappedConvexConcaveCreateFunc(0),
m_compoundCreateFunc(0),
m_swappedCompoundCreateFunc(0),
m_emptyCreateFunc(0)
{
	int i;

	m_emptyCreateFunc = new btEmptyAlgorithm::CreateFunc;
	for (i=0;i<MAX_BROADPHASE_COLLISION_TYPES;i++)
	{
		for (int j=0;j<MAX_BROADPHASE_COLLISION_TYPES;j++)
		{
			m_doubleDispatch[i][j] = m_emptyCreateFunc;
		}
	}
}

	
btCollisionDispatcher::btCollisionDispatcher (): 
	m_useIslands(true),
		m_count(0)
{
	int i;
	
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
	
	//ASSERT(gNumManifold < 65535);
	

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

	std::vector<btPersistentManifold*>::iterator i =
		std::find(m_manifoldsPtr.begin(), m_manifoldsPtr.end(), manifold);
	if (!(i == m_manifoldsPtr.end()))
	{
		std::swap(*i, m_manifoldsPtr.back());
		m_manifoldsPtr.pop_back();
		delete manifold;

	}
	
	
}

	

btCollisionAlgorithm* btCollisionDispatcher::findAlgorithm(btCollisionObject* body0,btCollisionObject* body1,btPersistentManifold* sharedManifold)
{
#define USE_DISPATCH_REGISTRY_ARRAY 1
#ifdef USE_DISPATCH_REGISTRY_ARRAY
	
	btCollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = this;
	ci.m_manifold = sharedManifold;
	btCollisionAlgorithm* algo = m_doubleDispatch[body0->m_collisionShape->getShapeType()][body1->m_collisionShape->getShapeType()]
	->CreateCollisionAlgorithm(ci,body0,body1);
#else
	btCollisionAlgorithm* algo = internalFindAlgorithm(body0,body1);
#endif //USE_DISPATCH_REGISTRY_ARRAY
	return algo;
}


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



btCollisionAlgorithm* btCollisionDispatcher::internalFindAlgorithm(btCollisionObject* body0,btCollisionObject* body1,btPersistentManifold* sharedManifold)
{
	m_count++;
	
	btCollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = this;
	
	if (body0->m_collisionShape->isConvex() && body1->m_collisionShape->isConvex() )
	{
		return new btConvexConvexAlgorithm(sharedManifold,ci,body0,body1);
	}

	if (body0->m_collisionShape->isConvex() && body1->m_collisionShape->isConcave())
	{
		return new btConvexConcaveCollisionAlgorithm(ci,body0,body1,false);
	}

	if (body1->m_collisionShape->isConvex() && body0->m_collisionShape->isConcave())
	{
		return new btConvexConcaveCollisionAlgorithm(ci,body0,body1,true);
	}

	if (body0->m_collisionShape->isCompound())
	{
		return new btCompoundCollisionAlgorithm(ci,body0,body1,false);
	} else
	{
		if (body1->m_collisionShape->isCompound())
		{
			return new btCompoundCollisionAlgorithm(ci,body0,body1,true);
		}
	}

	//failed to find an algorithm
	return new btEmptyAlgorithm(ci);
	
}

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
		
	if ((!body0->IsActive()) && (!body1->IsActive()))
		needsCollision = false;
	
	return needsCollision ;

}



///interface for iterating all overlapping collision pairs, no matter how those pairs are stored (array, set, map etc)
///this is useful for the collision dispatcher.
class btCollisionPairCallback : public btOverlapCallback
{
	btDispatcherInfo& m_dispatchInfo;
	btCollisionDispatcher*	m_dispatcher;
	int		m_dispatcherId;
public:

	btCollisionPairCallback(btDispatcherInfo& dispatchInfo,btCollisionDispatcher*	dispatcher,int		dispatcherId)
	:m_dispatchInfo(dispatchInfo),
	m_dispatcher(dispatcher),
	m_dispatcherId(dispatcherId)
	{
	}

	virtual bool	processOverlap(btBroadphasePair& pair)
	{
		btCollisionObject* body0 = (btCollisionObject*)pair.m_pProxy0->m_clientObject;
		btCollisionObject* body1 = (btCollisionObject*)pair.m_pProxy1->m_clientObject;

		if (!m_dispatcher->needsCollision(body0,body1))
			return false;

		//dispatcher will keep algorithms persistent in the collision pair
		if (!pair.m_algorithms[m_dispatcherId])
		{
			pair.m_algorithms[m_dispatcherId] = m_dispatcher->findAlgorithm(
				body0,
				body1);
		}

		if (pair.m_algorithms[m_dispatcherId])
		{
			btManifoldResult* resultOut = m_dispatcher->internalGetNewManifoldResult(body0,body1);
			if (m_dispatchInfo.m_dispatchFunc == 		btDispatcherInfo::DISPATCH_DISCRETE)
			{
				
				pair.m_algorithms[m_dispatcherId]->processCollision(body0,body1,m_dispatchInfo,resultOut);
			} else
			{
				float toi = pair.m_algorithms[m_dispatcherId]->calculateTimeOfImpact(body0,body1,m_dispatchInfo,resultOut);
				if (m_dispatchInfo.m_timeOfImpact > toi)
					m_dispatchInfo.m_timeOfImpact = toi;

			}
			m_dispatcher->internalReleaseManifoldResult(resultOut);
		}
		return false;

	}
};


void	btCollisionDispatcher::dispatchAllCollisionPairs(btOverlappingPairCache* pairCache,btDispatcherInfo& dispatchInfo)
{
	//m_blockedForChanges = true;

	int dispatcherId = getUniqueId();

	btCollisionPairCallback	collisionCallback(dispatchInfo,this,dispatcherId);

	pairCache->processAllOverlappingPairs(&collisionCallback);

	//m_blockedForChanges = false;

}


