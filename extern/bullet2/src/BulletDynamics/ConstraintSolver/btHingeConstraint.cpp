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
#include "LinearMath/btSimdMinMax.h"
#include <new>


btHingeConstraint::btHingeConstraint():
m_enableAngularMotor(false)
{
}

btHingeConstraint::btHingeConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB,
									 btVector3& axisInA,btVector3& axisInB)
									 :btTypedConstraint(rbA,rbB),
									 m_angularOnly(false),
									 m_enableAngularMotor(false)
{
	m_rbAFrame.getOrigin() = pivotInA;
	
	// since no frame is given, assume this to be zero angle and just pick rb transform axis
	btVector3 rbAxisA1 = rbA.getCenterOfMassTransform().getBasis().getColumn(0);
	btScalar projection = rbAxisA1.dot(axisInA);
	if (projection > SIMD_EPSILON)
		rbAxisA1 = rbAxisA1*projection - axisInA;
	 else
		rbAxisA1 = rbA.getCenterOfMassTransform().getBasis().getColumn(1);
	
	btVector3 rbAxisA2 = rbAxisA1.cross(axisInA);

	m_rbAFrame.getBasis().setValue( rbAxisA1.getX(),rbAxisA2.getX(),axisInA.getX(),
									rbAxisA1.getY(),rbAxisA2.getY(),axisInA.getY(),
									rbAxisA1.getZ(),rbAxisA2.getZ(),axisInA.getZ() );

	btQuaternion rotationArc = shortestArcQuat(axisInA,axisInB);
	btVector3 rbAxisB1 =  quatRotate(rotationArc,rbAxisA1);
	btVector3 rbAxisB2 =  rbAxisB1.cross(axisInB);
	
	
	m_rbBFrame.getOrigin() = pivotInB;
	m_rbBFrame.getBasis().setValue( rbAxisB1.getX(),rbAxisB2.getX(),-axisInB.getX(),
									rbAxisB1.getY(),rbAxisB2.getY(),-axisInB.getY(),
									rbAxisB1.getZ(),rbAxisB2.getZ(),-axisInB.getZ() );
	
	//start with free
	m_lowerLimit = btScalar(1e30);
	m_upperLimit = btScalar(-1e30);
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;
	m_limitSoftness = 0.9f;
	m_solveLimit = false;

}


btHingeConstraint::btHingeConstraint(btRigidBody& rbA,const btVector3& pivotInA,btVector3& axisInA)
:btTypedConstraint(rbA), m_angularOnly(false), m_enableAngularMotor(false)
{

	// since no frame is given, assume this to be zero angle and just pick rb transform axis
	// fixed axis in worldspace
	btVector3 rbAxisA1 = rbA.getCenterOfMassTransform().getBasis().getColumn(0);
	btScalar projection = rbAxisA1.dot(axisInA);
	if (projection > SIMD_EPSILON)
		rbAxisA1 = rbAxisA1*projection - axisInA;
	else
		rbAxisA1 = rbA.getCenterOfMassTransform().getBasis().getColumn(1);

	btVector3 rbAxisA2 = axisInA.cross(rbAxisA1);

	m_rbAFrame.getOrigin() = pivotInA;
	m_rbAFrame.getBasis().setValue( rbAxisA1.getX(),rbAxisA2.getX(),axisInA.getX(),
									rbAxisA1.getY(),rbAxisA2.getY(),axisInA.getY(),
									rbAxisA1.getZ(),rbAxisA2.getZ(),axisInA.getZ() );


	btVector3 axisInB = rbA.getCenterOfMassTransform().getBasis() * -axisInA;

	btQuaternion rotationArc = shortestArcQuat(axisInA,axisInB);
	btVector3 rbAxisB1 =  quatRotate(rotationArc,rbAxisA1);
	btVector3 rbAxisB2 = axisInB.cross(rbAxisB1);


	m_rbBFrame.getOrigin() = rbA.getCenterOfMassTransform()(pivotInA);
	m_rbBFrame.getBasis().setValue( rbAxisB1.getX(),rbAxisB2.getX(),axisInB.getX(),
									rbAxisB1.getY(),rbAxisB2.getY(),axisInB.getY(),
									rbAxisB1.getZ(),rbAxisB2.getZ(),axisInB.getZ() );
	
	//start with free
	m_lowerLimit = btScalar(1e30);
	m_upperLimit = btScalar(-1e30);
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;
	m_limitSoftness = 0.9f;
	m_solveLimit = false;
}

btHingeConstraint::btHingeConstraint(btRigidBody& rbA,btRigidBody& rbB, 
								     const btTransform& rbAFrame, const btTransform& rbBFrame)
:btTypedConstraint(rbA,rbB),m_rbAFrame(rbAFrame),m_rbBFrame(rbBFrame),
m_angularOnly(false),
m_enableAngularMotor(false)
{
	// flip axis
	m_rbBFrame.getBasis()[2][0] *= btScalar(-1.);
	m_rbBFrame.getBasis()[2][1] *= btScalar(-1.);
	m_rbBFrame.getBasis()[2][2] *= btScalar(-1.);

	//start with free
	m_lowerLimit = btScalar(1e30);
	m_upperLimit = btScalar(-1e30);
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;
	m_limitSoftness = 0.9f;
	m_solveLimit = false;
}			



btHingeConstraint::btHingeConstraint(btRigidBody& rbA, const btTransform& rbAFrame)
:btTypedConstraint(rbA),m_rbAFrame(rbAFrame),m_rbBFrame(rbAFrame),
m_angularOnly(false),
m_enableAngularMotor(false)
{
	// flip axis
	m_rbBFrame.getBasis()[2][0] *= btScalar(-1.);
	m_rbBFrame.getBasis()[2][1] *= btScalar(-1.);
	m_rbBFrame.getBasis()[2][2] *= btScalar(-1.);


	//start with free
	m_lowerLimit = btScalar(1e30);
	m_upperLimit = btScalar(-1e30);	
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;
	m_limitSoftness = 0.9f;
	m_solveLimit = false;
}

void	btHingeConstraint::buildJacobian()
{
	m_appliedImpulse = btScalar(0.);

	if (!m_angularOnly)
	{
		btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_rbAFrame.getOrigin();
		btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_rbBFrame.getOrigin();
		btVector3 relPos = pivotBInW - pivotAInW;

		btVector3 normal[3];
		if (relPos.length2() > SIMD_EPSILON)
		{
			normal[0] = relPos.normalized();
		}
		else
		{
			normal[0].setValue(btScalar(1.0),0,0);
		}

		btPlaneSpace1(normal[0], normal[1], normal[2]);

		for (int i=0;i<3;i++)
		{
			new (&m_jac[i]) btJacobianEntry(
				m_rbA.getCenterOfMassTransform().getBasis().transpose(),
				m_rbB.getCenterOfMassTransform().getBasis().transpose(),
				pivotAInW - m_rbA.getCenterOfMassPosition(),
				pivotBInW - m_rbB.getCenterOfMassPosition(),
				normal[i],
				m_rbA.getInvInertiaDiagLocal(),
				m_rbA.getInvMass(),
				m_rbB.getInvInertiaDiagLocal(),
				m_rbB.getInvMass());
		}
	}

	//calculate two perpendicular jointAxis, orthogonal to hingeAxis
	//these two jointAxis require equal angular velocities for both bodies

	//this is unused for now, it's a todo
	btVector3 jointAxis0local;
	btVector3 jointAxis1local;
	
	btPlaneSpace1(m_rbAFrame.getBasis().getColumn(2),jointAxis0local,jointAxis1local);

	getRigidBodyA().getCenterOfMassTransform().getBasis() * m_rbAFrame.getBasis().getColumn(2);
	btVector3 jointAxis0 = getRigidBodyA().getCenterOfMassTransform().getBasis() * jointAxis0local;
	btVector3 jointAxis1 = getRigidBodyA().getCenterOfMassTransform().getBasis() * jointAxis1local;
	btVector3 hingeAxisWorld = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_rbAFrame.getBasis().getColumn(2);
		
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


	// Compute limit information
	btScalar hingeAngle = getHingeAngle();  

	//set bias, sign, clear accumulator
	m_correction = btScalar(0.);
	m_limitSign = btScalar(0.);
	m_solveLimit = false;
	m_accLimitImpulse = btScalar(0.);

	if (m_lowerLimit < m_upperLimit)
	{
		if (hingeAngle <= m_lowerLimit*m_limitSoftness)
		{
			m_correction = (m_lowerLimit - hingeAngle);
			m_limitSign = 1.0f;
			m_solveLimit = true;
		} 
		else if (hingeAngle >= m_upperLimit*m_limitSoftness)
		{
			m_correction = m_upperLimit - hingeAngle;
			m_limitSign = -1.0f;
			m_solveLimit = true;
		}
	}

	//Compute K = J*W*J' for hinge axis
	btVector3 axisA =  getRigidBodyA().getCenterOfMassTransform().getBasis() *  m_rbAFrame.getBasis().getColumn(2);
	m_kHinge =   1.0f / (getRigidBodyA().computeAngularImpulseDenominator(axisA) +
			             getRigidBodyB().computeAngularImpulseDenominator(axisA));

}

void	btHingeConstraint::solveConstraint(btScalar	timeStep)
{

	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_rbAFrame.getOrigin();
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_rbBFrame.getOrigin();

	btScalar tau = btScalar(0.3);
	btScalar damping = btScalar(1.);

//linear part
	if (!m_angularOnly)
	{
		btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
		btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();

		btVector3 vel1 = m_rbA.getVelocityInLocalPoint(rel_pos1);
		btVector3 vel2 = m_rbB.getVelocityInLocalPoint(rel_pos2);
		btVector3 vel = vel1 - vel2;

		for (int i=0;i<3;i++)
		{		
			const btVector3& normal = m_jac[i].m_linearJointAxis;
			btScalar jacDiagABInv = btScalar(1.) / m_jac[i].getDiagonal();

			btScalar rel_vel;
			rel_vel = normal.dot(vel);
			//positional error (zeroth order error)
			btScalar depth = -(pivotAInW - pivotBInW).dot(normal); //this is the error projected on the normal
			btScalar impulse = depth*tau/timeStep  * jacDiagABInv -  rel_vel * jacDiagABInv;
			m_appliedImpulse += impulse;
			btVector3 impulse_vector = normal * impulse;
			m_rbA.applyImpulse(impulse_vector, pivotAInW - m_rbA.getCenterOfMassPosition());
			m_rbB.applyImpulse(-impulse_vector, pivotBInW - m_rbB.getCenterOfMassPosition());
		}
	}

	
	{
		///solve angular part

		// get axes in world space
		btVector3 axisA =  getRigidBodyA().getCenterOfMassTransform().getBasis() *  m_rbAFrame.getBasis().getColumn(2);
		btVector3 axisB =  getRigidBodyB().getCenterOfMassTransform().getBasis() *  m_rbBFrame.getBasis().getColumn(2);

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
				velrelOrthog *= (btScalar(1.)/denom) * m_relaxationFactor;
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

			// solve limit
			if (m_solveLimit)
			{
				btScalar amplitude = ( (angVelB - angVelA).dot( axisA )*m_relaxationFactor + m_correction* (btScalar(1.)/timeStep)*m_biasFactor  ) * m_limitSign;

				btScalar impulseMag = amplitude * m_kHinge;

				// Clamp the accumulated impulse
				btScalar temp = m_accLimitImpulse;
				m_accLimitImpulse = btMax(m_accLimitImpulse + impulseMag, 0.0f );
				impulseMag = m_accLimitImpulse - temp;


				btVector3 impulse = axisA * impulseMag * m_limitSign;
				m_rbA.applyTorqueImpulse(impulse);
				m_rbB.applyTorqueImpulse(-impulse);
			}
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

			btScalar unclippedMotorImpulse = m_kHinge * motor_relvel;;
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

btScalar btHingeConstraint::getHingeAngle()
{
	const btVector3 refAxis0  = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_rbAFrame.getBasis().getColumn(0);
	const btVector3 refAxis1  = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_rbAFrame.getBasis().getColumn(1);
	const btVector3 swingAxis = getRigidBodyB().getCenterOfMassTransform().getBasis() * m_rbBFrame.getBasis().getColumn(1);

	return btAtan2Fast( swingAxis.dot(refAxis0), swingAxis.dot(refAxis1)  );
}
