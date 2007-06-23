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

btScalar gLinearAirDamping = btScalar(1.);
//'temporarily' global variables
btScalar	gDeactivationTime = btScalar(2.);
bool	gDisableDeactivation = false;

btScalar gLinearSleepingThreshold = btScalar(0.8);
btScalar gAngularSleepingThreshold = btScalar(1.0);
static int uniqueId = 0;

btRigidBody::btRigidBody(btScalar mass, btMotionState* motionState, btCollisionShape* collisionShape, const btVector3& localInertia,btScalar linearDamping,btScalar angularDamping,btScalar friction,btScalar restitution)
: 
	m_linearVelocity(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_angularVelocity(btScalar(0.),btScalar(0.),btScalar(0.)),
	m_angularFactor(btScalar(1.)),
	m_gravity(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_totalForce(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_totalTorque(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_linearDamping(btScalar(0.)),
	m_angularDamping(btScalar(0.5)),
	m_optionalMotionState(motionState),
	m_contactSolverType(0),
	m_frictionSolverType(0)
{

	if (motionState)
	{
		motionState->getWorldTransform(m_worldTransform);
	} else
	{
		m_worldTransform = btTransform::getIdentity();
	}

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

#ifdef OBSOLETE_MOTIONSTATE_LESS
btRigidBody::btRigidBody( btScalar mass,const btTransform& worldTransform,btCollisionShape* collisionShape,const btVector3& localInertia,btScalar linearDamping,btScalar angularDamping,btScalar friction,btScalar restitution)
: 
	m_gravity(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_totalForce(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_totalTorque(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_linearVelocity(btScalar(0.0), btScalar(0.0), btScalar(0.0)),
	m_angularVelocity(btScalar(0.),btScalar(0.),btScalar(0.)),
	m_linearDamping(btScalar(0.)),
	m_angularDamping(btScalar(0.5)),
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

#endif //OBSOLETE_MOTIONSTATE_LESS




#define EXPERIMENTAL_JITTER_REMOVAL 1
#ifdef EXPERIMENTAL_JITTER_REMOVAL
//Bullet 2.20b has experimental damping code to reduce jitter just before objects fall asleep/deactivate
//doesn't work very well yet (value 0 disabled this damping)
//note there this influences deactivation thresholds!
btScalar gClippedAngvelThresholdSqr = btScalar(0.01);
btScalar	gClippedLinearThresholdSqr = btScalar(0.01);
#endif //EXPERIMENTAL_JITTER_REMOVAL

btScalar	gJitterVelocityDampingFactor = btScalar(0.7);

void btRigidBody::predictIntegratedTransform(btScalar timeStep,btTransform& predictedTransform) 
{

#ifdef EXPERIMENTAL_JITTER_REMOVAL
	//if (wantsSleeping())
	{
		//clip to avoid jitter
		if ((m_angularVelocity.length2() < gClippedAngvelThresholdSqr) &&
			(m_linearVelocity.length2() < gClippedLinearThresholdSqr))
		{
			m_angularVelocity *= gJitterVelocityDampingFactor;
			m_linearVelocity *= gJitterVelocityDampingFactor;
		}
	}
	
#endif //EXPERIMENTAL_JITTER_REMOVAL

	btTransformUtil::integrateTransform(m_worldTransform,m_linearVelocity,m_angularVelocity,timeStep,predictedTransform);
}

void			btRigidBody::saveKinematicState(btScalar timeStep)
{
	//todo: clamp to some (user definable) safe minimum timestep, to limit maximum angular/linear velocities
	if (timeStep != btScalar(0.))
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
	if (m_inverseMass != btScalar(0.0))
	{
		m_gravity = acceleration * (btScalar(1.0) / m_inverseMass);
	}
}






void btRigidBody::setDamping(btScalar lin_damping, btScalar ang_damping)
{
	m_linearDamping = GEN_clamped(lin_damping, (btScalar)btScalar(0.0), (btScalar)btScalar(1.0));
	m_angularDamping = GEN_clamped(ang_damping, (btScalar)btScalar(0.0), (btScalar)btScalar(1.0));
}



#include <stdio.h>


void btRigidBody::applyForces(btScalar step)
{
	if (isStaticOrKinematicObject())
		return;
	
	applyCentralForce(m_gravity);	
	
	m_linearVelocity *= GEN_clamped((btScalar(1.) - step * gLinearAirDamping * m_linearDamping), (btScalar)btScalar(0.0), (btScalar)btScalar(1.0));
	m_angularVelocity *= GEN_clamped((btScalar(1.) - step * m_angularDamping), (btScalar)btScalar(0.0), (btScalar)btScalar(1.0));

#define FORCE_VELOCITY_DAMPING 1
#ifdef FORCE_VELOCITY_DAMPING
	btScalar speed = m_linearVelocity.length();
	if (speed < m_linearDamping)
	{
		btScalar dampVel = btScalar(0.005);
		if (speed > dampVel)
		{
			btVector3 dir = m_linearVelocity.normalized();
			m_linearVelocity -=  dir * dampVel;
		} else
		{
			m_linearVelocity.setValue(btScalar(0.),btScalar(0.),btScalar(0.));
		}
	}

	btScalar angSpeed = m_angularVelocity.length();
	if (angSpeed < m_angularDamping)
	{
		btScalar angDampVel = btScalar(0.005);
		if (angSpeed > angDampVel)
		{
			btVector3 dir = m_angularVelocity.normalized();
			m_angularVelocity -=  dir * angDampVel;
		} else
		{
			m_angularVelocity.setValue(btScalar(0.),btScalar(0.),btScalar(0.));
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
	if (mass == btScalar(0.))
	{
		m_collisionFlags |= btCollisionObject::CF_STATIC_OBJECT;
		m_inverseMass = btScalar(0.);
	} else
	{
		m_collisionFlags &= (~btCollisionObject::CF_STATIC_OBJECT);
		m_inverseMass = btScalar(1.0) / mass;
	}
	
	m_invInertiaLocal.setValue(inertia.x() != btScalar(0.0) ? btScalar(1.0) / inertia.x(): btScalar(0.0),
				   inertia.y() != btScalar(0.0) ? btScalar(1.0) / inertia.y(): btScalar(0.0),
				   inertia.z() != btScalar(0.0) ? btScalar(1.0) / inertia.z(): btScalar(0.0));

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
	btScalar angvel = m_angularVelocity.length();
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
	m_interpolationWorldTransform = xform;//m_worldTransform;
	m_interpolationLinearVelocity = getLinearVelocity();
	m_interpolationAngularVelocity = getAngularVelocity();
	m_worldTransform = xform;
	updateInertiaTensor();
}



