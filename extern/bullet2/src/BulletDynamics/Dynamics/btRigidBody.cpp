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

#include "btRigidBody.h"
#include "BulletCollision/CollisionShapes/btConvexShape.h"
#include "LinearMath/btMinMax.h"
#include <LinearMath/btTransformUtil.h>
#include <LinearMath/btMotionState.h>

float gLinearAirDamping = 1.f;
//'temporarily' global variables
float	gDeactivationTime = 2.f;
bool	gDisableDeactivation = false;

float gLinearSleepingThreshold = 0.8f;
float gAngularSleepingThreshold = 1.0f;
static int uniqueId = 0;

btRigidBody::btRigidBody(float mass, btMotionState* motionState, btCollisionShape* collisionShape, const btVector3& localInertia,btScalar linearDamping,btScalar angularDamping,btScalar friction,btScalar restitution)
: 
	m_gravity(0.0f, 0.0f, 0.0f),
	m_totalForce(0.0f, 0.0f, 0.0f),
	m_totalTorque(0.0f, 0.0f, 0.0f),
	m_linearVelocity(0.0f, 0.0f, 0.0f),
	m_angularVelocity(0.f,0.f,0.f),
	m_linearDamping(0.f),
	m_angularDamping(0.5f),
	m_optionalMotionState(motionState),
	m_contactSolverType(0),
	m_frictionSolverType(0)
{

	motionState->getWorldTransform(m_worldTransform);

	m_interpolationWorldTransform = m_worldTransform;
	m_interpolationLinearVelocity.setValue(0,0,0);
	m_interpolationAngularVelocity.setValue(0,0,0);
	
	//moved to btCollisionObject
	m_friction = friction;
	m_restitution = restitution;

	m_collisionShape = collisionShape;
	m_debugBodyId = uniqueId++;
	
	//m_internalOwner is to allow upcasting from collision object to rigid body
	m_internalOwner = this;

	setMassProps(mass, localInertia);
    setDamping(linearDamping, angularDamping);
	updateInertiaTensor();

}


btRigidBody::btRigidBody( float mass,const btTransform& worldTransform,btCollisionShape* collisionShape,const btVector3& localInertia,btScalar linearDamping,btScalar angularDamping,btScalar friction,btScalar restitution)
: 
	m_gravity(0.0f, 0.0f, 0.0f),
	m_totalForce(0.0f, 0.0f, 0.0f),
	m_totalTorque(0.0f, 0.0f, 0.0f),
	m_linearVelocity(0.0f, 0.0f, 0.0f),
	m_angularVelocity(0.f,0.f,0.f),
	m_linearDamping(0.f),
	m_angularDamping(0.5f),
	m_optionalMotionState(0),
	m_contactSolverType(0),
	m_frictionSolverType(0)
{
	
	m_worldTransform = worldTransform;
	m_interpolationWorldTransform = m_worldTransform;
	m_interpolationLinearVelocity.setValue(0,0,0);
	m_interpolationAngularVelocity.setValue(0,0,0);

	//moved to btCollisionObject
	m_friction = friction;
	m_restitution = restitution;

	m_collisionShape = collisionShape;
	m_debugBodyId = uniqueId++;
	
	//m_internalOwner is to allow upcasting from collision object to rigid body
	m_internalOwner = this;

	setMassProps(mass, localInertia);
    setDamping(linearDamping, angularDamping);
	updateInertiaTensor();

}

#define EXPERIMENTAL_JITTER_REMOVAL 1
#ifdef EXPERIMENTAL_JITTER_REMOVAL
//Bullet 2.20b has experimental code to reduce jitter just before objects fall asleep/deactivate
//doesn't work very well yet (value 0 only reduces performance a bit, no difference in functionality)
float gClippedAngvelThresholdSqr = 0.f;
float	gClippedLinearThresholdSqr = 0.f;
#endif //EXPERIMENTAL_JITTER_REMOVAL

void btRigidBody::predictIntegratedTransform(btScalar timeStep,btTransform& predictedTransform) 
{

#ifdef EXPERIMENTAL_JITTER_REMOVAL
	//clip to avoid jitter
	if (m_angularVelocity.length2() < gClippedAngvelThresholdSqr)
	{
		m_angularVelocity.setValue(0,0,0);
		printf("clipped!\n");
	}
	
	if (m_linearVelocity.length2() < gClippedLinearThresholdSqr)
	{
		m_linearVelocity.setValue(0,0,0);
		printf("clipped!\n");
	}
#endif //EXPERIMENTAL_JITTER_REMOVAL

	btTransformUtil::integrateTransform(m_worldTransform,m_linearVelocity,m_angularVelocity,timeStep,predictedTransform);
}

void			btRigidBody::saveKinematicState(btScalar timeStep)
{
	//todo: clamp to some (user definable) safe minimum timestep, to limit maximum angular/linear velocities
	if (timeStep != 0.f)
	{
		//if we use motionstate to synchronize world transforms, get the new kinematic/animated world transform
		if (getMotionState())
			getMotionState()->getWorldTransform(m_worldTransform);
		btVector3 linVel,angVel;
		
		btTransformUtil::calculateVelocity(m_interpolationWorldTransform,m_worldTransform,timeStep,m_linearVelocity,m_angularVelocity);
		m_interpolationLinearVelocity = m_linearVelocity;
		m_interpolationAngularVelocity = m_angularVelocity;
		m_interpolationWorldTransform = m_worldTransform;
		//printf("angular = %f %f %f\n",m_angularVelocity.getX(),m_angularVelocity.getY(),m_angularVelocity.getZ());
	}
}
	
void	btRigidBody::getAabb(btVector3& aabbMin,btVector3& aabbMax) const
{
	getCollisionShape()->getAabb(m_worldTransform,aabbMin,aabbMax);
}




void btRigidBody::setGravity(const btVector3& acceleration) 
{
	if (m_inverseMass != 0.0f)
	{
		m_gravity = acceleration * (1.0f / m_inverseMass);
	}
}






void btRigidBody::setDamping(btScalar lin_damping, btScalar ang_damping)
{
	m_linearDamping = GEN_clamped(lin_damping, 0.0f, 1.0f);
	m_angularDamping = GEN_clamped(ang_damping, 0.0f, 1.0f);
}



#include <stdio.h>


void btRigidBody::applyForces(btScalar step)
{
	if (isStaticOrKinematicObject())
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
			btVector3 dir = m_linearVelocity.normalized();
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
			btVector3 dir = m_angularVelocity.normalized();
			m_angularVelocity -=  dir * angDampVel;
		} else
		{
			m_angularVelocity.setValue(0.f,0.f,0.f);
		}
	}
#endif //FORCE_VELOCITY_DAMPING
	
}

void btRigidBody::proceedToTransform(const btTransform& newTrans)
{
	setCenterOfMassTransform( newTrans );
}
	

void btRigidBody::setMassProps(btScalar mass, const btVector3& inertia)
{
	if (mass == 0.f)
	{
		m_collisionFlags |= btCollisionObject::CF_STATIC_OBJECT;
		m_inverseMass = 0.f;
	} else
	{
		m_collisionFlags &= (~btCollisionObject::CF_STATIC_OBJECT);
		m_inverseMass = 1.0f / mass;
	}
	
	m_invInertiaLocal.setValue(inertia[0] != 0.0f ? 1.0f / inertia[0]: 0.0f,
				   inertia[1] != 0.0f ? 1.0f / inertia[1]: 0.0f,
				   inertia[2] != 0.0f ? 1.0f / inertia[2]: 0.0f);

}

	

void btRigidBody::updateInertiaTensor() 
{
	m_invInertiaTensorWorld = m_worldTransform.getBasis().scaled(m_invInertiaLocal) * m_worldTransform.getBasis().transpose();
}


void btRigidBody::integrateVelocities(btScalar step) 
{
	if (isStaticOrKinematicObject())
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

btQuaternion btRigidBody::getOrientation() const
{
		btQuaternion orn;
		m_worldTransform.getBasis().getRotation(orn);
		return orn;
}
	
	
void btRigidBody::setCenterOfMassTransform(const btTransform& xform)
{
	m_interpolationWorldTransform = m_worldTransform;
	m_interpolationLinearVelocity = getLinearVelocity();
	m_interpolationAngularVelocity = getAngularVelocity();
	m_worldTransform = xform;
	updateInertiaTensor();
}



