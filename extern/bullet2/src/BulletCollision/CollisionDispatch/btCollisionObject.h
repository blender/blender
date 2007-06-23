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

#include "../../LinearMath/btTransform.h"

//island management, m_activationState1
#define ACTIVE_TAG 1
#define ISLAND_SLEEPING 2
#define WANTS_DEACTIVATION 3
#define DISABLE_DEACTIVATION 4
#define DISABLE_SIMULATION 5

struct	btBroadphaseProxy;
class	btCollisionShape;
#include "../../LinearMath/btMotionState.h"



/// btCollisionObject can be used to manage collision detection objects. 
/// btCollisionObject maintains all information that is needed for a collision detection: Shape, Transform and AABB proxy.
/// They can be added to the btCollisionWorld.
ATTRIBUTE_ALIGNED16(class)	btCollisionObject
{

protected:

	btTransform	m_worldTransform;

	///m_interpolationWorldTransform is used for CCD and interpolation
	///it can be either previous or future (predicted) transform
	btTransform	m_interpolationWorldTransform;
	//those two are experimental: just added for bullet time effect, so you can still apply impulses (directly modifying velocities) 
	//without destroying the continuous interpolated motion (which uses this interpolation velocities)
	btVector3	m_interpolationLinearVelocity;
	btVector3	m_interpolationAngularVelocity;
	btBroadphaseProxy*		m_broadphaseHandle;
	btCollisionShape*		m_collisionShape;

	int				m_collisionFlags;

	int				m_islandTag1;
	int				m_companionId;

	int				m_activationState1;
	btScalar			m_deactivationTime;

	btScalar		m_friction;
	btScalar		m_restitution;

	///users can point to their objects, m_userPointer is not used by Bullet, see setUserPointer/getUserPointer
	void*			m_userObjectPointer;

	///m_internalOwner is reserved to point to Bullet's btRigidBody. Don't use this, use m_userObjectPointer instead.
	void*			m_internalOwner;

	///time of impact calculation
	btScalar			m_hitFraction; 
	
	///Swept sphere radius (0.0 by default), see btConvexConvexAlgorithm::
	btScalar			m_ccdSweptSphereRadius;

	/// Don't do continuous collision detection if square motion (in one step) is less then m_ccdSquareMotionThreshold
	btScalar			m_ccdSquareMotionThreshold;
	
	char	m_pad[8];

public:

	enum CollisionFlags
	{
		CF_STATIC_OBJECT= 1,
		CF_KINEMATIC_OBJECT= 2,
		CF_NO_CONTACT_RESPONSE = 4,
		CF_CUSTOM_MATERIAL_CALLBACK = 8//this allows per-triangle material (friction/restitution)
	};


	inline bool mergesSimulationIslands() const
	{
		///static objects, kinematic and object without contact response don't merge islands
		return  ((m_collisionFlags & (CF_STATIC_OBJECT | CF_KINEMATIC_OBJECT | CF_NO_CONTACT_RESPONSE) )==0);
	}


	inline bool		isStaticObject() const {
		return (m_collisionFlags & CF_STATIC_OBJECT) != 0;
	}

	inline bool		isKinematicObject() const
	{
		return (m_collisionFlags & CF_KINEMATIC_OBJECT) != 0;
	}

	inline bool		isStaticOrKinematicObject() const
	{
		return (m_collisionFlags & (CF_KINEMATIC_OBJECT | CF_STATIC_OBJECT)) != 0 ;
	}

	inline bool		hasContactResponse() const {
		return (m_collisionFlags & CF_NO_CONTACT_RESPONSE)==0;
	}

	
	btCollisionObject();


	void	setCollisionShape(btCollisionShape* collisionShape)
	{
		m_collisionShape = collisionShape;
	}

	const btCollisionShape*	getCollisionShape() const
	{
		return m_collisionShape;
	}

	btCollisionShape*	getCollisionShape()
	{
		return m_collisionShape;
	}

	


	int	getActivationState() const { return m_activationState1;}
	
	void setActivationState(int newState);

	void	setDeactivationTime(btScalar time)
	{
		m_deactivationTime = time;
	}
	btScalar	getDeactivationTime() const
	{
		return m_deactivationTime;
	}

	void forceActivationState(int newState);

	void	activate(bool forceActivation = false);

	inline bool isActive() const
	{
		return ((getActivationState() != ISLAND_SLEEPING) && (getActivationState() != DISABLE_SIMULATION));
	}

		void	setRestitution(btScalar rest)
	{
		m_restitution = rest;
	}
	btScalar	getRestitution() const
	{
		return m_restitution;
	}
	void	setFriction(btScalar frict)
	{
		m_friction = frict;
	}
	btScalar	getFriction() const
	{
		return m_friction;
	}

	///reserved for Bullet internal usage
	void*	getInternalOwner()
	{
		return m_internalOwner;
	}

	const void*	getInternalOwner() const
	{
		return m_internalOwner;
	}

	btTransform&	getWorldTransform()
	{
		return m_worldTransform;
	}

	const btTransform&	getWorldTransform() const
	{
		return m_worldTransform;
	}

	void	setWorldTransform(const btTransform& worldTrans)
	{
		m_worldTransform = worldTrans;
	}


	btBroadphaseProxy*	getBroadphaseHandle()
	{
		return m_broadphaseHandle;
	}

	const btBroadphaseProxy*	getBroadphaseHandle() const
	{
		return m_broadphaseHandle;
	}

	void	setBroadphaseHandle(btBroadphaseProxy* handle)
	{
		m_broadphaseHandle = handle;
	}


	const btTransform&	getInterpolationWorldTransform() const
	{
		return m_interpolationWorldTransform;
	}

	btTransform&	getInterpolationWorldTransform()
	{
		return m_interpolationWorldTransform;
	}

	void	setInterpolationWorldTransform(const btTransform&	trans)
	{
		m_interpolationWorldTransform = trans;
	}


	const btVector3&	getInterpolationLinearVelocity() const
	{
		return m_interpolationLinearVelocity;
	}

	const btVector3&	getInterpolationAngularVelocity() const
	{
		return m_interpolationAngularVelocity;
	}

	const int getIslandTag() const
	{
		return	m_islandTag1;
	}

	void	setIslandTag(int tag)
	{
		m_islandTag1 = tag;
	}

	const int getCompanionId() const
	{
		return	m_companionId;
	}

	void	setCompanionId(int id)
	{
		m_companionId = id;
	}

	const btScalar			getHitFraction() const
	{
		return m_hitFraction; 
	}

	void	setHitFraction(btScalar hitFraction)
	{
		m_hitFraction = hitFraction;
	}

	
	const int	getCollisionFlags() const
	{
		return m_collisionFlags;
	}

	void	setCollisionFlags(int flags)
	{
		m_collisionFlags = flags;
	}
	
	///Swept sphere radius (0.0 by default), see btConvexConvexAlgorithm::
	btScalar			getCcdSweptSphereRadius() const
	{
		return m_ccdSweptSphereRadius;
	}

	///Swept sphere radius (0.0 by default), see btConvexConvexAlgorithm::
	void	setCcdSweptSphereRadius(btScalar radius)
	{
		m_ccdSweptSphereRadius = radius;
	}

	btScalar 	getCcdSquareMotionThreshold() const
	{
		return m_ccdSquareMotionThreshold;
	}


	/// Don't do continuous collision detection if square motion (in one step) is less then m_ccdSquareMotionThreshold
	void	setCcdSquareMotionThreshold(btScalar ccdSquareMotionThreshold)
	{
		m_ccdSquareMotionThreshold = ccdSquareMotionThreshold;
	}

	///users can point to their objects, userPointer is not used by Bullet
	void*	getUserPointer() const
	{
		return m_userObjectPointer;
	}
	
	///users can point to their objects, userPointer is not used by Bullet
	void	setUserPointer(void* userPointer)
	{
		m_userObjectPointer = userPointer;
	}

}
;

#endif //COLLISION_OBJECT_H
