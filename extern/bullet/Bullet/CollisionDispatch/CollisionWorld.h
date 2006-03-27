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
	virtual ~CollisionWorld() {}

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

	virtual void	PerformDiscreteCollisionDetection();

};


#endif //COLLISION_WORLD_H
