#ifndef RIGIDBODY_H
#define RIGIDBODY_H

#include <vector>
#include <SimdPoint3.h>
#include <SimdTransform.h>

class CollisionShape;
struct MassProps;
typedef SimdScalar dMatrix3[4*3];


/// RigidBody class for RigidBody Dynamics
/// 
class RigidBody  {
public:

	RigidBody(const MassProps& massProps,SimdScalar linearDamping,SimdScalar angularDamping,SimdScalar friction,SimdScalar restitution);

	void			proceedToTransform(const SimdTransform& newTrans); 
	
	bool			mergesSimulationIslands() const;
	
	/// continuous collision detection needs prediction
	void			predictIntegratedTransform(SimdScalar step, SimdTransform& predictedTransform) const;
	
	void			applyForces(SimdScalar step);
	
	void			setGravity(const SimdVector3& acceleration);  
	
	void			setDamping(SimdScalar lin_damping, SimdScalar ang_damping);
	
	CollisionShape*	GetCollisionShape() { return m_collisionShape; }
	
	void			setMassProps(SimdScalar mass, const SimdVector3& inertia);
	
	SimdScalar		getInvMass() const { return m_inverseMass; }
	const SimdMatrix3x3& getInvInertiaTensorWorld() const { return m_invInertiaTensorWorld; }
		
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
	
	const SimdPoint3&     getCenterOfMassPosition() const { return m_worldTransform.getOrigin(); }
	SimdQuaternion getOrientation() const;
	
	const SimdTransform&  getCenterOfMassTransform() const { return m_worldTransform; }
	const SimdVector3&   getLinearVelocity() const { return m_linearVelocity; }
	const SimdVector3&    getAngularVelocity() const { return m_angularVelocity; }
	

	void setLinearVelocity(const SimdVector3& lin_vel);
	void setAngularVelocity(const SimdVector3& ang_vel) { m_angularVelocity = ang_vel; }

	SimdVector3 getVelocityInLocalPoint(const SimdVector3& rel_pos) const
	{
		return m_linearVelocity + m_angularVelocity.cross(rel_pos);
	}

	void translate(const SimdVector3& v) 
	{
		m_worldTransform.getOrigin() += v; 
	}

	void	SetCollisionShape(CollisionShape* mink);

	void	getAabb(SimdVector3& aabbMin,SimdVector3& aabbMax) const;

	int	GetActivationState() const { return m_activationState1;}
	void SetActivationState(int newState);

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

private:
	SimdTransform	m_worldTransform;
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

	CollisionShape*	m_collisionShape;

	
public:
	/// for ode solver-binding
	dMatrix3		m_R;//temp
	dMatrix3		m_I;
	dMatrix3		m_invI;
	int				m_islandTag1;//temp
	int				m_activationState1;//temp
	int				m_odeTag;
	SimdVector3		m_tacc;//temp
	SimdVector3		m_facc;
	SimdScalar		m_hitFraction; //time of impact calculation

	int	m_debugBodyId;
};



#endif
