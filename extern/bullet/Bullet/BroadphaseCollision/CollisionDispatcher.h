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
#ifndef COLLISION_DISPATCHER_H
#define COLLISION_DISPATCHER_H

class CollisionAlgorithm;
struct BroadphaseProxy;
class RigidBody;

enum CollisionDispatcherId
{
	RIGIDBODY_DISPATCHER = 0,
	RAGDOLL_DISPATCHER
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
		m_useContinuous(false)
	{

	}
	float	m_timeStep;
	int		m_stepCount;
	int		m_dispatchFunc;
	float	m_timeOfImpact;
	bool	m_useContinuous;
	
};

/// Collision Dispatcher can be used in combination with broadphase and collision algorithms.
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


};


#endif //COLLISION_DISPATCHER_H
