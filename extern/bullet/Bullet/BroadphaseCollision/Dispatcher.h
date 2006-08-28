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

class CollisionAlgorithm;
struct BroadphaseProxy;
class RigidBody;
struct	CollisionObject;
class ManifoldResult;
class OverlappingPairCache;

enum CollisionDispatcherId
{
	RIGIDBODY_DISPATCHER = 0,
	USERCALLBACK_DISPATCHER
};

class PersistentManifold;

struct DispatcherInfo
{
	enum DispatchFunc
	{
		DISPATCH_DISCRETE = 1,
		DISPATCH_CONTINUOUS
	};
	DispatcherInfo()
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
	class IDebugDraw*	m_debugDraw;
	bool	m_enableSatConvex;
	
};

/// Dispatcher can be used in combination with broadphase to dispatch overlapping pairs.
/// For example for pairwise collision detection or user callbacks (game logic).
class Dispatcher
{


public:
	virtual ~Dispatcher() ;

	virtual CollisionAlgorithm* FindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1) = 0;

	//
	// asume dispatchers to have unique id's in the range [0..max dispacher]
	//
	virtual int GetUniqueId() = 0;

	virtual PersistentManifold*	GetNewManifold(void* body0,void* body1)=0;

	virtual void ReleaseManifold(PersistentManifold* manifold)=0;

	virtual void ClearManifold(PersistentManifold* manifold)=0;

	virtual bool	NeedsCollision(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1) = 0;

	virtual bool	NeedsResponse(const CollisionObject& colObj0,const CollisionObject& colObj1)=0;

	virtual	ManifoldResult* GetNewManifoldResult(CollisionObject* obj0,CollisionObject* obj1,PersistentManifold* manifold) =0;

	virtual	void	ReleaseManifoldResult(ManifoldResult*)=0;

	virtual void	DispatchAllCollisionPairs(struct BroadphasePair* pairs,int numPairs,DispatcherInfo& dispatchInfo)=0;

	virtual int GetNumManifolds() const = 0;

	virtual PersistentManifold* GetManifoldByIndexInternal(int index) = 0;

};


#endif //_DISPATCHER_H
