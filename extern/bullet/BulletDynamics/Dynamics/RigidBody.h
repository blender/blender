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

#ifndef RIGIDBODY_H
#define RIGIDBODY_H

#include <vector>
#include <SimdPoint3.h>
#include <SimdTransform.h>
#include "BroadphaseCollision/BroadphaseProxy.h"


#include "CollisionDispatch/CollisionObject.h"

class CollisionShape;
struct MassProps;
typedef SimdScalar dMatrix3[4*3];

extern float gLinearAirDamping;
extern bool gUseEpa;

#define MAX_RIGIDBODIES 8192


/// RigidBody class for RigidBody Dynamics
/// 
class RigidBody  : public CollisionObject
{
public:

	RigidBody(const MassProps& massProps,SimdScalar linearDamping,SimdScalar angularDamping,SimdScalar friction,SimdScalar restitution);

	void			proceedToTransform(const SimdTransform& newTrans); 
	
	
	/// continuous collision detection needs prediction
	void			predictIntegratedTransform(SimdScalar step, SimdTransform& predictedTransform) const;
	
	void			saveKinematicState(SimdScalar step);
	

	void			applyForces(SimdScalar step);
	
	void			setGravity(const SimdVector3& acceleration);  
	
	void			setDamping(SimdScalar lin_damping, SimdScalar ang_damping);
	
	inline const CollisionShape*	GetCollisionShape() const {
		return m_collisionShape;
	}

	inline CollisionShape*	GetCollisionShape() {
			return m_collisionShape;
	}
	
	void			setMassProps(SimdScalar mass, const SimdVector3& inertia);
	
	SimdScalar		getInvMass() const { return m_inverseMass; }
	const SimdMatrix3x3& getInvInertiaTensorWorld() const { 
		return m_invInertiaTensorWorld; 
	}
		
	void			integrateVelocities(SimdScalar step);

	void			setCenterOfMassTransform(const SimdTransform& xform);

	void			applyCentralForce(const SimdVector3& force)
	{
		m_totalForce += force;
	}
    
	const SimdVector3& getInvInertiaDiagLocal()
	{
		return m_invInertiaLocal;
	};

	void	setInvInertiaDiagLocal(const SimdVector3& diagInvInertia)
	{
		m_invInertiaLocal = diagInvInertia;
	}

	void	applyTorque(const SimdVector3& torque)
	{
		m_totalTorque += torque;
	}
	
	void	applyForce(const SimdVector3& force, const SimdVector3& rel_pos) 
	{
		applyCentralForce(force);
		applyTorque(rel_pos.cross(force));
	}
	
	void applyCentralImpulse(const SimdVector3& impulse)
	{
		m_linearVelocity += impulse * m_inverseMass;
	}
	
  	void applyTorqueImpulse(const SimdVector3& torque)
	{
		if (!IsStatic())
			m_angularVelocity += m_invInertiaTensorWorld * torque;

	}
	
	void applyImpulse(const SimdVector3& impulse, const SimdVector3& rel_pos) 
	{
		if (m_inverseMass != 0.f)
		{
			applyCentralImpulse(impulse);
			applyTorqueImpulse(rel_pos.cross(impulse));
		}
	}
	
	void clearForces() 
	{
		m_totalForce.setValue(0.0f, 0.0f, 0.0f);
		m_totalTorque.setValue(0.0f, 0.0f, 0.0f);
	}
	
	void updateInertiaTensor();    
	
	const SimdPoint3&     getCenterOfMassPosition() const { 
		return m_worldTransform.getOrigin(); 
	}
	SimdQuaternion getOrientation() const;
	
	const SimdTransform&  getCenterOfMassTransform() const { 
		return m_worldTransform; 
	}
	const SimdVector3&   getLinearVelocity() const { 
		return m_linearVelocity; 
	}
	const SimdVector3&    getAngularVelocity() const { 
		return m_angularVelocity; 
	}
	

	void setLinearVelocity(const SimdVector3& lin_vel);
	void setAngularVelocity(const SimdVector3& ang_vel) { 
		if (!IsStatic())
		{
			m_angularVelocity = ang_vel; 
		}
	}

	SimdVector3 getVelocityInLocalPoint(const SimdVector3& rel_pos) const
	{
		//we also calculate lin/ang velocity for kinematic objects
		return m_linearVelocity + m_angularVelocity.cross(rel_pos);

		//for kinematic objects, we could also use use:
		//		return 	(m_worldTransform(rel_pos) - m_interpolationWorldTransform(rel_pos)) / m_kinematicTimeStep;
	}

	void translate(const SimdVector3& v) 
	{
		m_worldTransform.getOrigin() += v; 
	}

	
	void	getAabb(SimdVector3& aabbMin,SimdVector3& aabbMax) const;


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

	
	inline float ComputeImpulseDenominator(const SimdPoint3& pos, const SimdVector3& normal) const
	{
		SimdVector3 r0 = pos - getCenterOfMassPosition();

		SimdVector3 c0 = (r0).cross(normal);

		SimdVector3 vec = (c0 * getInvInertiaTensorWorld()).cross(r0);

		return m_inverseMass + normal.dot(vec);

	}

	inline float ComputeAngularImpulseDenominator(const SimdVector3& axis) const
	{
		SimdVector3 vec = axis * getInvInertiaTensorWorld();
		return axis.dot(vec);
	}



private:
	
	SimdMatrix3x3	m_invInertiaTensorWorld;
	SimdVector3		m_gravity;	
	SimdVector3		m_invInertiaLocal;
	SimdVector3		m_totalForce;
	SimdVector3		m_totalTorque;
//	SimdQuaternion	m_orn1;
	
	SimdVector3		m_linearVelocity;
	
	SimdVector3		m_angularVelocity;
	
	SimdScalar		m_linearDamping;
	SimdScalar		m_angularDamping;
	SimdScalar		m_inverseMass;

	SimdScalar		m_friction;
	SimdScalar		m_restitution;

	SimdScalar		m_kinematicTimeStep;

	BroadphaseProxy*	m_broadphaseProxy;


	


	
public:
	const BroadphaseProxy*	GetBroadphaseProxy() const
	{
		return m_broadphaseProxy;
	}
	BroadphaseProxy*	GetBroadphaseProxy() 
	{
		return m_broadphaseProxy;
	}
	void	SetBroadphaseProxy(BroadphaseProxy* broadphaseProxy)
	{
		m_broadphaseProxy = broadphaseProxy;
	}
	

	/// for ode solver-binding
	dMatrix3		m_R;//temp
	dMatrix3		m_I;
	dMatrix3		m_invI;

	int				m_odeTag;
	float		m_padding[1024];
	SimdVector3		m_tacc;//temp
	SimdVector3		m_facc;
	

	int	m_debugBodyId;
};



#endif
