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

#include "LinearMath/btTransform.h"

//island management, m_activationState1
#define ACTIVE_TAG 1
#define ISLAND_SLEEPING 2
#define WANTS_DEACTIVATION 3
#define DISABLE_DEACTIVATION 4
#define DISABLE_SIMULATION 5

struct	btBroadphaseProxy;
class	btCollisionShape;
#include "LinearMath/btMotionState.h"



/// btCollisionObject can be used to manage collision detection objects. 
/// btCollisionObject maintains all information that is needed for a collision detection: Shape, Transform and AABB proxy.
/// They can be added to the btCollisionWorld.
struct	btCollisionObject
{
	btTransform	m_worldTransform;
	btBroadphaseProxy*	m_broadphaseHandle;
	btCollisionShape*		m_collisionShape;

	///m_interpolationWorldTransform is used for CCD and interpolation
	///it can be either previous or future (predicted) transform
	btTransform	m_interpolationWorldTransform;
	//those two are experimental: just added for bullet time effect, so you can still apply impulses (directly modifying velocities) 
	//without destroying the continuous interpolated motion (which uses this interpolation velocities)
	btVector3	m_interpolationLinearVelocity;
	btVector3	m_interpolationAngularVelocity;


	enum CollisionFlags
	{
		CF_STATIC_OBJECT= 1,
		CF_KINEMATIC_OJBECT= 2,
		CF_NO_CONTACT_RESPONSE = 4,
		CF_CUSTOM_MATERIAL_CALLBACK = 8,//this allows per-triangle material (friction/restitution)
	};

	int				m_collisionFlags;

	int				m_islandTag1;
	int				m_activationState1;
	float			m_deactivationTime;

	btScalar		m_friction;
	btScalar		m_restitution;

	///users can point to their objects, m_userPointer is not used by Bullet
	void*			m_userObjectPointer;

	///m_internalOwner is reserved to point to Bullet's btRigidBody. Don't use this, use m_userObjectPointer instead.
	void*			m_internalOwner;

	///time of impact calculation
	float			m_hitFraction; 
	
	///Swept sphere radius (0.0 by default), see btConvexConvexAlgorithm::
	float			m_ccdSweptSphereRadius;

	/// Don't do continuous collision detection if square motion (in one step) is less then m_ccdSquareMotionThreshold
	float			m_ccdSquareMotionThreshold;

	inline bool mergesSimulationIslands() const
	{
		///static objects, kinematic and object without contact response don't merge islands
		return  ((m_collisionFlags & (CF_STATIC_OBJECT | CF_KINEMATIC_OJBECT | CF_NO_CONTACT_RESPONSE) )==0);
	}


	inline bool		isStaticObject() const {
		return (m_collisionFlags & CF_STATIC_OBJECT) != 0;
	}

	inline bool		isKinematicObject() const
	{
		return (m_collisionFlags & CF_KINEMATIC_OJBECT) != 0;
	}

	inline bool		isStaticOrKinematicObject() const
	{
		return (m_collisionFlags & (CF_KINEMATIC_OJBECT | CF_STATIC_OBJECT)) != 0 ;
	}

	inline bool		hasContactResponse() const {
		return (m_collisionFlags & CF_NO_CONTACT_RESPONSE)==0;
	}

	


	btCollisionObject();


	void	SetCollisionShape(btCollisionShape* collisionShape)
	{
		m_collisionShape = collisionShape;
	}

	int	GetActivationState() const { return m_activationState1;}
	
	void SetActivationState(int newState);

	void ForceActivationState(int newState);

	void	activate();

	inline bool IsActive() const
	{
		return ((GetActivationState() != ISLAND_SLEEPING) && (GetActivationState() != DISABLE_SIMULATION));
	}

		void	setRestitution(float rest)
	{
		m_restitution = rest;
	}
	float	getRestitution() const
	{
		return m_restitution;
	}
	void	setFriction(float frict)
	{
		m_friction = frict;
	}
	float	getFriction() const
	{
		return m_friction;
	}


};

#endif //COLLISION_OBJECT_H
