/*
 * Copyright (c) 2002-2006 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#ifndef COLLISION_WORLD_H
#define COLLISION_WORLD_H

struct CollisionObject;
class CollisionDispatcher;
class BroadphaseInterface;

#include <vector>

///CollisionWorld is interface and container for the collision detection
class CollisionWorld
{

	std::vector<CollisionObject*>	m_collisionObjects;
	
	CollisionDispatcher*	m_dispatcher;

	BroadphaseInterface*	m_broadphase;

	public:

	CollisionWorld(CollisionDispatcher* dispatcher,BroadphaseInterface* broadphase)
		:m_dispatcher(dispatcher),
		m_broadphase(broadphase)
	{

	}

	virtual	void	UpdateActivationState();

	BroadphaseInterface*	GetBroadphase()
	{
		return m_broadphase;
	}

	CollisionDispatcher*	GetDispatcher()
	{
		return m_dispatcher;
	}

	int	GetNumCollisionObjects() const
	{
		return m_collisionObjects.size();
	}

	void	AddCollisionObject(CollisionObject* collisionObject);

	void	RemoveCollisionObject(CollisionObject* collisionObject);

};


#endif //COLLISION_WORLD_H
