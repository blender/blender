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

#ifndef _DISPATCHER_H
#define _DISPATCHER_H

class btCollisionAlgorithm;
struct btBroadphaseProxy;
class btRigidBody;
struct	btCollisionObject;
class btOverlappingPairCache;

enum btCollisionDispatcherId
{
	RIGIDBODY_DISPATCHER = 0,
	USERCALLBACK_DISPATCHER
};

class btPersistentManifold;

struct btDispatcherInfo
{
	enum DispatchFunc
	{
		DISPATCH_DISCRETE = 1,
		DISPATCH_CONTINUOUS
	};
	btDispatcherInfo()
		:m_dispatchFunc(DISPATCH_DISCRETE),
		m_timeOfImpact(1.f),
		m_useContinuous(false),
		m_debugDraw(0),
		m_enableSatConvex(false)
	{

	}
	float	m_timeStep;
	int		m_stepCount;
	int		m_dispatchFunc;
	float	m_timeOfImpact;
	bool	m_useContinuous;
	class btIDebugDraw*	m_debugDraw;
	bool	m_enableSatConvex;
	
};

/// btDispatcher can be used in combination with broadphase to dispatch overlapping pairs.
/// For example for pairwise collision detection or user callbacks (game logic).
class btDispatcher
{


public:
	virtual ~btDispatcher() ;

	virtual btCollisionAlgorithm* findAlgorithm(btCollisionObject* body0,btCollisionObject* body1,btPersistentManifold* sharedManifold=0) = 0;

	//
	// asume dispatchers to have unique id's in the range [0..max dispacher]
	//
	virtual int getUniqueId() = 0;

	virtual btPersistentManifold*	getNewManifold(void* body0,void* body1)=0;

	virtual void releaseManifold(btPersistentManifold* manifold)=0;

	virtual void clearManifold(btPersistentManifold* manifold)=0;

	virtual bool	needsCollision(btCollisionObject* body0,btCollisionObject* body1) = 0;

	virtual bool	needsResponse(btCollisionObject* body0,btCollisionObject* body1)=0;

	virtual void	dispatchAllCollisionPairs(btOverlappingPairCache* pairCache,btDispatcherInfo& dispatchInfo)=0;

	virtual int getNumManifolds() const = 0;

	virtual btPersistentManifold* getManifoldByIndexInternal(int index) = 0;

};


#endif //_DISPATCHER_H
