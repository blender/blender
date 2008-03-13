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

#ifndef COLLISION__DISPATCHER_H
#define COLLISION__DISPATCHER_H

#include "../BroadphaseCollision/btDispatcher.h"
#include "../NarrowPhaseCollision/btPersistentManifold.h"

#include "../CollisionDispatch/btManifoldResult.h"

#include "../BroadphaseCollision/btBroadphaseProxy.h"
#include "../../LinearMath/btAlignedObjectArray.h"

class btIDebugDraw;
class btOverlappingPairCache;


#include "btCollisionCreateFunc.h"

#define USE_DISPATCH_REGISTRY_ARRAY 1

class btCollisionDispatcher;
///user can override this nearcallback for collision filtering and more finegrained control over collision detection
typedef void (*btNearCallback)(btBroadphasePair& collisionPair, btCollisionDispatcher& dispatcher, btDispatcherInfo& dispatchInfo);


///btCollisionDispatcher supports algorithms that handle ConvexConvex and ConvexConcave collision pairs.
///Time of Impact, Closest Points and Penetration Depth.
class btCollisionDispatcher : public btDispatcher
{
	int m_count;
	
	btAlignedObjectArray<btPersistentManifold*>	m_manifoldsPtr;

	bool m_useIslands;
	
	btManifoldResult	m_defaultManifoldResult;

	btNearCallback		m_nearCallback;
	
	btCollisionAlgorithmCreateFunc* m_doubleDispatch[MAX_BROADPHASE_COLLISION_TYPES][MAX_BROADPHASE_COLLISION_TYPES];
	
	btCollisionAlgorithmCreateFunc* internalFindCreateFunc(int proxyType0,int proxyType1);

	//default CreationFunctions, filling the m_doubleDispatch table
	btCollisionAlgorithmCreateFunc*	m_convexConvexCreateFunc;
	btCollisionAlgorithmCreateFunc*	m_convexConcaveCreateFunc;
	btCollisionAlgorithmCreateFunc*	m_swappedConvexConcaveCreateFunc;
	btCollisionAlgorithmCreateFunc*	m_compoundCreateFunc;
	btCollisionAlgorithmCreateFunc*	m_swappedCompoundCreateFunc;
	btCollisionAlgorithmCreateFunc*   m_emptyCreateFunc;

#ifndef USE_DISPATCH_REGISTRY_ARRAY
	btCollisionAlgorithm* internalFindAlgorithm(btCollisionObject* body0,btCollisionObject* body1,btPersistentManifold* sharedManifold = 0);
#endif //USE_DISPATCH_REGISTRY_ARRAY

public:

	///registerCollisionCreateFunc allows registration of custom/alternative collision create functions
	void	registerCollisionCreateFunc(int proxyType0,int proxyType1, btCollisionAlgorithmCreateFunc* createFunc);

	int	getNumManifolds() const
	{ 
		return int( m_manifoldsPtr.size());
	}

	btPersistentManifold**	getInternalManifoldPointer()
	{
		return &m_manifoldsPtr[0];
	}

	 btPersistentManifold* getManifoldByIndexInternal(int index)
	{
		return m_manifoldsPtr[index];
	}

	 const btPersistentManifold* getManifoldByIndexInternal(int index) const
	{
		return m_manifoldsPtr[index];
	}

	///the default constructor creates/register default collision algorithms, for convex, compound and concave shape support
	btCollisionDispatcher ();

	///a special constructor that doesn't create/register the default collision algorithms
	btCollisionDispatcher(bool noDefaultAlgorithms);

	virtual ~btCollisionDispatcher();

	virtual btPersistentManifold*	getNewManifold(void* b0,void* b1);
	
	virtual void releaseManifold(btPersistentManifold* manifold);


	virtual void clearManifold(btPersistentManifold* manifold);

			
	btCollisionAlgorithm* findAlgorithm(btCollisionObject* body0,btCollisionObject* body1,btPersistentManifold* sharedManifold = 0);
		
	virtual bool	needsCollision(btCollisionObject* body0,btCollisionObject* body1);
	
	virtual bool	needsResponse(btCollisionObject* body0,btCollisionObject* body1);
	
	virtual void	dispatchAllCollisionPairs(btOverlappingPairCache* pairCache,btDispatcherInfo& dispatchInfo);

	void	setNearCallback(btNearCallback	nearCallback)
	{
		m_nearCallback = nearCallback; 
	}

	btNearCallback	getNearCallback() const
	{
		return m_nearCallback;
	}

	//by default, Bullet will use this near callback
	static void  defaultNearCallback(btBroadphasePair& collisionPair, btCollisionDispatcher& dispatcher, btDispatcherInfo& dispatchInfo);

};

#endif //COLLISION__DISPATCHER_H

