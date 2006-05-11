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

#include "RigidBody.h"
#include "MassProps.h"
#include "CollisionShapes/ConvexShape.h"
#include "GEN_MinMax.h"
#include <SimdTransformUtil.h>

float gLinearAirDamping = 1.f;

static int uniqueId = 0;

RigidBody::RigidBody( const MassProps& massProps,SimdScalar linearDamping,SimdScalar angularDamping,SimdScalar friction,SimdScalar restitution)
: 
	m_gravity(0.0f, 0.0f, 0.0f),
	m_totalForce(0.0f, 0.0f, 0.0f),
	m_totalTorque(0.0f, 0.0f, 0.0f),
	m_linearVelocity(0.0f, 0.0f, 0.0f),
	m_angularVelocity(0.f,0.f,0.f),
	m_linearDamping(0.f),
	m_angularDamping(0.5f),
	m_friction(friction),
	m_restitution(restitution),
	m_kinematicTimeStep(0.f)
{
	m_debugBodyId = uniqueId++;
	
	setMassProps(massProps.m_mass, massProps.m_inertiaLocal);
    setDamping(linearDamping, angularDamping);
	m_worldTransform.setIdentity();
	updateInertiaTensor();

}


void RigidBody::setLinearVelocity(const SimdVector3& lin_vel)
{ 

	m_linearVelocity = lin_vel; 
}


void RigidBody::predictIntegratedTransform(SimdScalar timeStep,SimdTransform& predictedTransform) const
{
	SimdTransformUtil::IntegrateTransform(m_worldTransform,m_linearVelocity,m_angularVelocity,timeStep,predictedTransform);
}

void			RigidBody::saveKinematicState(SimdScalar timeStep)
{
	
	if (m_kinematicTimeStep)
	{
		SimdVector3 linVel,angVel;
		SimdTransformUtil::CalculateVelocity(m_interpolationWorldTransform,m_worldTransform,m_kinematicTimeStep,m_linearVelocity,m_angularVelocity);
		//printf("angular = %f %f %f\n",m_angularVelocity.getX(),m_angularVelocity.getY(),m_angularVelocity.getZ());
	}
	

	m_interpolationWorldTransform = m_worldTransform;
	
	m_kinematicTimeStep = timeStep;
}
	
void	RigidBody::getAabb(SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	GetCollisionShape()->GetAabb(m_worldTransform,aabbMin,aabbMax);
}




void RigidBody::setGravity(const SimdVector3& acceleration) 
{
	if (m_inverseMass != 0.0f)
	{
		m_gravity = acceleration * (1.0f / m_inverseMass);
	}
}






void RigidBody::setDamping(SimdScalar lin_damping, SimdScalar ang_damping)
{
	m_linearDamping = GEN_clamped(lin_damping, 0.0f, 1.0f);
	m_angularDamping = GEN_clamped(ang_damping, 0.0f, 1.0f);
}



#include <stdio.h>


void RigidBody::applyForces(SimdScalar step)
{
	if (IsStatic())
		return;

	
	applyCentralForce(m_gravity);	
	
	m_linearVelocity *= GEN_clamped((1.f - step * gLinearAirDamping * m_linearDamping), 0.0f, 1.0f);
	m_angularVelocity *= GEN_clamped((1.f - step * m_angularDamping), 0.0f, 1.0f);

#define FORCE_VELOCITY_DAMPING 1
#ifdef FORCE_VELOCITY_DAMPING
	float speed = m_linearVelocity.length();
	if (speed < m_linearDamping)
	{
		float dampVel = 0.005f;
		if (speed > dampVel)
		{
			SimdVector3 dir = m_linearVelocity.normalized();
			m_linearVelocity -=  dir * dampVel;
		} else
		{
			m_linearVelocity.setValue(0.f,0.f,0.f);
		}
	}

	float angSpeed = m_angularVelocity.length();
	if (angSpeed < m_angularDamping)
	{
		float angDampVel = 0.005f;
		if (angSpeed > angDampVel)
		{
			SimdVector3 dir = m_angularVelocity.normalized();
			m_angularVelocity -=  dir * angDampVel;
		} else
		{
			m_angularVelocity.setValue(0.f,0.f,0.f);
		}
	}
#endif //FORCE_VELOCITY_DAMPING
	
}

void RigidBody::proceedToTransform(const SimdTransform& newTrans)
{
	setCenterOfMassTransform( newTrans );
}
	

void RigidBody::setMassProps(SimdScalar mass, const SimdVector3& inertia)
{
	if (mass == 0.f)
	{
		m_collisionFlags = CollisionObject::isStatic;
		m_inverseMass = 0.f;
	} else
	{
		m_collisionFlags = 0;
		m_inverseMass = 1.0f / mass;
	}
	

	m_invInertiaLocal.setValue(inertia[0] != 0.0f ? 1.0f / inertia[0]: 0.0f,
						   inertia[1] != 0.0f ? 1.0f / inertia[1]: 0.0f,
						   inertia[2] != 0.0f ? 1.0f / inertia[2]: 0.0f);

}

	

void RigidBody::updateInertiaTensor() 
{
	m_invInertiaTensorWorld = m_worldTransform.getBasis().scaled(m_invInertiaLocal) * m_worldTransform.getBasis().transpose();
}


void RigidBody::integrateVelocities(SimdScalar step) 
{
	if (IsStatic())
		return;

	m_linearVelocity += m_totalForce * (m_inverseMass * step);
	m_angularVelocity += m_invInertiaTensorWorld * m_totalTorque * step;

#define MAX_ANGVEL SIMD_HALF_PI
	/// clamp angular velocity. collision calculations will fail on higher angular velocities	
	float angvel = m_angularVelocity.length();
	if (angvel*step > MAX_ANGVEL)
	{
		m_angularVelocity *= (MAX_ANGVEL/step) /angvel;
	}

	clearForces();
}

SimdQuaternion RigidBody::getOrientation() const
{
		SimdQuaternion orn;
		m_worldTransform.getBasis().getRotation(orn);
		return orn;
}
	
	
void RigidBody::setCenterOfMassTransform(const SimdTransform& xform)
{
	m_worldTransform = xform;
	updateInertiaTensor();
}



