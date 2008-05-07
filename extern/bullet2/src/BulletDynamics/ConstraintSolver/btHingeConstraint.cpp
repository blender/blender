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


#include "btHingeConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <new>

btHingeConstraint::btHingeConstraint():
m_enableAngularMotor(false)
{
}

btHingeConstraint::btHingeConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB,
								 btVector3& axisInA,btVector3& axisInB)
:btTypedConstraint(rbA,rbB),m_pivotInA(pivotInA),m_pivotInB(pivotInB),
m_axisInA(axisInA),
m_axisInB(-axisInB),
m_angularOnly(false),
m_enableAngularMotor(false)
{

}


btHingeConstraint::btHingeConstraint(btRigidBody& rbA,const btVector3& pivotInA,btVector3& axisInA)
:btTypedConstraint(rbA),m_pivotInA(pivotInA),m_pivotInB(rbA.getCenterOfMassTransform()(pivotInA)),
m_axisInA(axisInA),
//fixed axis in worldspace
m_axisInB(rbA.getCenterOfMassTransform().getBasis() * -axisInA),
m_angularOnly(false),
m_enableAngularMotor(false)
{
	
}

void	btHingeConstraint::buildJacobian()
{
	m_appliedImpulse = btScalar(0.);

	btVector3	normal(0,0,0);

	if (!m_angularOnly)
	{
		for (int i=0;i<3;i++)
		{
			normal[i] = 1;
			new (&m_jac[i]) btJacobianEntry(
				m_rbA.getCenterOfMassTransform().getBasis().transpose(),
				m_rbB.getCenterOfMassTransform().getBasis().transpose(),
				m_rbA.getCenterOfMassTransform()*m_pivotInA - m_rbA.getCenterOfMassPosition(),
				m_rbB.getCenterOfMassTransform()*m_pivotInB - m_rbB.getCenterOfMassPosition(),
				normal,
				m_rbA.getInvInertiaDiagLocal(),
				m_rbA.getInvMass(),
				m_rbB.getInvInertiaDiagLocal(),
				m_rbB.getInvMass());
			normal[i] = 0;
		}
	}

	//calculate two perpendicular jointAxis, orthogonal to hingeAxis
	//these two jointAxis require equal angular velocities for both bodies

	//this is unused for now, it's a todo
	btVector3 jointAxis0local;
	btVector3 jointAxis1local;
	
	btPlaneSpace1(m_axisInA,jointAxis0local,jointAxis1local);

	getRigidBodyA().getCenterOfMassTransform().getBasis() * m_axisInA;
	btVector3 jointAxis0 = getRigidBodyA().getCenterOfMassTransform().getBasis() * jointAxis0local;
	btVector3 jointAxis1 = getRigidBodyA().getCenterOfMassTransform().getBasis() * jointAxis1local;
	btVector3 hingeAxisWorld = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_axisInA;
		
	new (&m_jacAng[0])	btJacobianEntry(jointAxis0,
		m_rbA.getCenterOfMassTransform().getBasis().transpose(),
		m_rbB.getCenterOfMassTransform().getBasis().transpose(),
		m_rbA.getInvInertiaDiagLocal(),
		m_rbB.getInvInertiaDiagLocal());

	new (&m_jacAng[1])	btJacobianEntry(jointAxis1,
		m_rbA.getCenterOfMassTransform().getBasis().transpose(),
		m_rbB.getCenterOfMassTransform().getBasis().transpose(),
		m_rbA.getInvInertiaDiagLocal(),
		m_rbB.getInvInertiaDiagLocal());

	new (&m_jacAng[2])	btJacobianEntry(hingeAxisWorld,
		m_rbA.getCenterOfMassTransform().getBasis().transpose(),
		m_rbB.getCenterOfMassTransform().getBasis().transpose(),
		m_rbA.getInvInertiaDiagLocal(),
		m_rbB.getInvInertiaDiagLocal());



}

void	btHingeConstraint::solveConstraint(btScalar	timeStep)
{

	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_pivotInA;
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_pivotInB;

	btVector3 normal(0,0,0);
	btScalar tau = btScalar(0.3);
	btScalar damping = btScalar(1.);

//linear part
	if (!m_angularOnly)
	{
		for (int i=0;i<3;i++)
		{		
			normal[i] = 1;
			btScalar jacDiagABInv = btScalar(1.) / m_jac[i].getDiagonal();

			btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
			btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
			
			btVector3 vel1 = m_rbA.getVelocityInLocalPoint(rel_pos1);
			btVector3 vel2 = m_rbB.getVelocityInLocalPoint(rel_pos2);
			btVector3 vel = vel1 - vel2;
			btScalar rel_vel;
			rel_vel = normal.dot(vel);
			//positional error (zeroth order error)
			btScalar depth = -(pivotAInW - pivotBInW).dot(normal); //this is the error projected on the normal
			btScalar impulse = depth*tau/timeStep  * jacDiagABInv -  damping * rel_vel * jacDiagABInv * damping;
			m_appliedImpulse += impulse;
			btVector3 impulse_vector = normal * impulse;
			m_rbA.applyImpulse(impulse_vector, pivotAInW - m_rbA.getCenterOfMassPosition());
			m_rbB.applyImpulse(-impulse_vector, pivotBInW - m_rbB.getCenterOfMassPosition());
			
			normal[i] = 0;
		}
	}

	
	{
		///solve angular part

		// get axes in world space
		btVector3 axisA = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_axisInA;
		btVector3 axisB = getRigidBodyB().getCenterOfMassTransform().getBasis() * m_axisInB;

		const btVector3& angVelA = getRigidBodyA().getAngularVelocity();
		const btVector3& angVelB = getRigidBodyB().getAngularVelocity();

		btVector3 angVelAroundHingeAxisA = axisA * axisA.dot(angVelA);
		btVector3 angVelAroundHingeAxisB = axisB * axisB.dot(angVelB);

		btVector3 angAorthog = angVelA - angVelAroundHingeAxisA;
		btVector3 angBorthog = angVelB - angVelAroundHingeAxisB;
		btVector3 velrelOrthog = angAorthog-angBorthog;
		{
			//solve orthogonal angular velocity correction
			btScalar relaxation = btScalar(1.);
			btScalar len = velrelOrthog.length();
			if (len > btScalar(0.00001))
			{
				btVector3 normal = velrelOrthog.normalized();
				btScalar denom = getRigidBodyA().computeAngularImpulseDenominator(normal) +
					getRigidBodyB().computeAngularImpulseDenominator(normal);
				// scale for mass and relaxation
				//todo:  expose this 0.9 factor to developer
				velrelOrthog *= (btScalar(1.)/denom) * btScalar(0.9);
			}

			//solve angular positional correction
			btVector3 angularError = -axisA.cross(axisB) *(btScalar(1.)/timeStep);
			btScalar len2 = angularError.length();
			if (len2>btScalar(0.00001))
			{
				btVector3 normal2 = angularError.normalized();
				btScalar denom2 = getRigidBodyA().computeAngularImpulseDenominator(normal2) +
						getRigidBodyB().computeAngularImpulseDenominator(normal2);
				angularError *= (btScalar(1.)/denom2) * relaxation;
			}

			m_rbA.applyTorqueImpulse(-velrelOrthog+angularError);
			m_rbB.applyTorqueImpulse(velrelOrthog-angularError);
		}

		//apply motor
		if (m_enableAngularMotor)
		{
			//todo: add limits too
			btVector3 angularLimit(0,0,0);

			btVector3 velrel = angVelAroundHingeAxisA - angVelAroundHingeAxisB;
			btScalar projRelVel = velrel.dot(axisA);

			btScalar desiredMotorVel = m_motorTargetVelocity;
			btScalar motor_relvel = desiredMotorVel - projRelVel;

			btScalar denom3 = getRigidBodyA().computeAngularImpulseDenominator(axisA) +
					getRigidBodyB().computeAngularImpulseDenominator(axisA);

			btScalar unclippedMotorImpulse = (btScalar(1.)/denom3) * motor_relvel;;
			//todo: should clip against accumulated impulse
			btScalar clippedMotorImpulse = unclippedMotorImpulse > m_maxMotorImpulse ? m_maxMotorImpulse : unclippedMotorImpulse;
			clippedMotorImpulse = clippedMotorImpulse < -m_maxMotorImpulse ? -m_maxMotorImpulse : clippedMotorImpulse;
			btVector3 motorImp = clippedMotorImpulse * axisA;

			m_rbA.applyTorqueImpulse(motorImp+angularLimit);
			m_rbB.applyTorqueImpulse(-motorImp-angularLimit);
			
		}
	}

}

void	btHingeConstraint::updateRHS(btScalar	timeStep)
{
	(void)timeStep;

}

