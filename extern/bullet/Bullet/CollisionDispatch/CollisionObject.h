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

#ifndef COLLISION_OBJECT_H
#define COLLISION_OBJECT_H

#include "SimdTransform.h"

//island management, m_activationState1
#define ACTIVE_TAG 1
#define ISLAND_SLEEPING 2
#define WANTS_DEACTIVATION 3
#define DISABLE_DEACTIVATION 4

struct	BroadphaseProxy;
class	CollisionShape;

struct	CollisionObject
{
	SimdTransform	m_worldTransform;
	
	//m_interpolationWorldTransform is used for CCD and interpolation
	//it can be either previous or future (predicted) transform
	SimdTransform	m_interpolationWorldTransform;
	
	enum CollisionFlags
	{
		isStatic = 1,
		noContactResponse = 2,

	};

	int				m_collisionFlags;

	int				m_islandTag1;
	int				m_activationState1;
	float			m_deactivationTime;

	BroadphaseProxy*	m_broadphaseHandle;
	CollisionShape*		m_collisionShape;

	void*			m_userPointer;//not use by Bullet internally

	//time of impact calculation
	float			m_hitFraction; 

	bool			mergesSimulationIslands() const;

	inline bool		IsStatic() const {
		return m_collisionFlags & isStatic;
	}

	inline bool		HasContactResponse() {
		return !(m_collisionFlags & noContactResponse);
	}

	


	CollisionObject();


	void	SetCollisionShape(CollisionShape* collisionShape)
	{
		m_collisionShape = collisionShape;
	}

	int	GetActivationState() const { return m_activationState1;}
	
	void SetActivationState(int newState);

	void ForceActivationState(int newState);

	void	activate();



};

#endif //COLLISION_OBJECT_H
