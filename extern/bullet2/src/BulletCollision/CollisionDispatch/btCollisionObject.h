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
#include "LinearMath/btAlignedAllocator.h"



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
	
	///m_rootCollisionShape is temporarily used to store the original collision shape
	///The m_collisionShape might be temporarily replaced by a child collision shape during collision detection purposes
	///If it is NULL, the m_collisionShape is not temporarily replaced.
	btCollisionShape*		m_rootCollisionShape;

	int				m_collisionFlags;

	int				m_islandTag1;
	int				m_companionId;

	int				m_activationState1;
	btScalar			m_deactivationTime;

	btScalar		m_friction;
	btScalar		m_restitution;

	///users can point to their objects, m_userPointer is not used by Bullet, see setUserPointer/getUserPointer
	void*			m_userObjectPointer;

	///m_internalType is reserved to distinguish Bullet's btCollisionObject, btRigidBody, btSoftBody etc.
	///do not assign your own m_internalType unless you write a new dynamics object class.
	int				m_internalType;

	///time of impact calculation
	btScalar		m_hitFraction; 
	
	///Swept sphere radius (0.0 by default), see btConvexConvexAlgorithm::
	btScalar		m_ccdSweptSphereRadius;

	/// Don't do continuous collision detection if the motion (in one step) is less then m_ccdMotionThreshold
	btScalar		m_ccdMotionThreshold;
	
	/// If some object should have elaborate collision filtering by sub-classes
	bool			m_checkCollideWith;

	char	m_pad[7];

	virtual bool	checkCollideWithOverride(btCollisionObject* /* co */)
	{
		return true;
	}

public:

	BT_DECLARE_ALIGNED_ALLOCATOR();

	enum CollisionFlags
	{
		CF_STATIC_OBJECT= 1,
		CF_KINEMATIC_OBJECT= 2,
		CF_NO_CONTACT_RESPONSE = 4,
		CF_CUSTOM_MATERIAL_CALLBACK = 8//this allows per-triangle material (friction/restitution)
	};

	enum	CollisionObjectTypes
	{
		CO_COLLISION_OBJECT =1,
		CO_RIGID_BODY,
		CO_SOFT_BODY
	};

	SIMD_FORCE_INLINE bool mergesSimulationIslands() const
	{
		///static objects, kinematic and object without contact response don't merge islands
		return  ((m_collisionFlags & (CF_STATIC_OBJECT | CF_KINEMATIC_OBJECT | CF_NO_CONTACT_RESPONSE) )==0);
	}


	SIMD_FORCE_INLINE bool		isStaticObject() const {
		return (m_collisionFlags & CF_STATIC_OBJECT) != 0;
	}

	SIMD_FORCE_INLINE bool		isKinematicObject() const
	{
		return (m_collisionFlags & CF_KINEMATIC_OBJECT) != 0;
	}

	SIMD_FORCE_INLINE bool		isStaticOrKinematicObject() const
	{
		return (m_collisionFlags & (CF_KINEMATIC_OBJECT | CF_STATIC_OBJECT)) != 0 ;
	}

	SIMD_FORCE_INLINE bool		hasContactResponse() const {
		return (m_collisionFlags & CF_NO_CONTACT_RESPONSE)==0;
	}

	
	btCollisionObject();

	virtual ~btCollisionObject();

	virtual void	setCollisionShape(btCollisionShape* collisionShape)
	{
		m_collisionShape = collisionShape;
		m_rootCollisionShape = collisionShape;
	}

	SIMD_FORCE_INLINE const btCollisionShape*	getCollisionShape() const
	{
		return m_collisionShape;
	}

	SIMD_FORCE_INLINE btCollisionShape*	getCollisionShape()
	{
		return m_collisionShape;
	}

	SIMD_FORCE_INLINE const btCollisionShape*	getRootCollisionShape() const
	{
		return m_rootCollisionShape;
	}

	SIMD_FORCE_INLINE btCollisionShape*	getRootCollisionShape()
	{
		return m_rootCollisionShape;
	}

	///Avoid using this internal API call
	///internalSetTemporaryCollisionShape is used to temporary replace the actual collision shape by a child collision shape.
	void	internalSetTemporaryCollisionShape(btCollisionShape* collisionShape)
	{
		m_collisionShape = collisionShape;
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
	int	getInternalType() const
	{
		return m_internalType;
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

	void	setInterpolationLinearVelocity(const btVector3& linvel)
	{
		m_interpolationLinearVelocity = linvel;
	}

	void	setInterpolationAngularVelocity(const btVector3& angvel)
	{
		m_interpolationAngularVelocity = angvel;
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

	btScalar 	getCcdMotionThreshold() const
	{
		return m_ccdMotionThreshold;
	}

	btScalar 	getCcdSquareMotionThreshold() const
	{
		return m_ccdMotionThreshold*m_ccdMotionThreshold;
	}



	/// Don't do continuous collision detection if the motion (in one step) is less then m_ccdMotionThreshold
	void	setCcdMotionThreshold(btScalar ccdMotionThreshold)
	{
		m_ccdMotionThreshold = ccdMotionThreshold*ccdMotionThreshold;
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


	inline bool checkCollideWith(btCollisionObject* co)
	{
		if (m_checkCollideWith)
			return checkCollideWithOverride(co);

		return true;
	}
};

#endif //COLLISION_OBJECT_H
