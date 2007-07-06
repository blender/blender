/*
Bullet Continuous Collision Detection and Physics Library
btConeTwistConstraint is Copyright (c) 2007 Starbreeze Studios

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Written by: Marcus Hennix
*/


#include "btConeTwistConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include "LinearMath/btSimdMinMax.h"
#include <new>

btConeTwistConstraint::btConeTwistConstraint()
{
}


btConeTwistConstraint::btConeTwistConstraint(btRigidBody& rbA,btRigidBody& rbB, 
											 const btTransform& rbAFrame,const btTransform& rbBFrame)
											 :btTypedConstraint(rbA,rbB),m_rbAFrame(rbAFrame),m_rbBFrame(rbBFrame),
											 m_angularOnly(false)
{
	// flip axis for correct angles
	m_rbBFrame.getBasis()[1][0] *= btScalar(-1.);
	m_rbBFrame.getBasis()[1][1] *= btScalar(-1.);
	m_rbBFrame.getBasis()[1][2] *= btScalar(-1.);

	m_swingSpan1 = btScalar(1e30);
	m_swingSpan2 = btScalar(1e30);
	m_twistSpan  = btScalar(1e30);
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;

	m_solveTwistLimit = false;
	m_solveSwingLimit = false;

}

btConeTwistConstraint::btConeTwistConstraint(btRigidBody& rbA,const btTransform& rbAFrame)
											:btTypedConstraint(rbA),m_rbAFrame(rbAFrame),
											 m_angularOnly(false)
{
	m_rbBFrame = m_rbAFrame;
	
	// flip axis for correct angles
	m_rbBFrame.getBasis()[1][0] *= btScalar(-1.);
	m_rbBFrame.getBasis()[1][1] *= btScalar(-1.);
	m_rbBFrame.getBasis()[1][2] *= btScalar(-1.);

	m_rbBFrame.getBasis()[2][0] *= btScalar(-1.);
	m_rbBFrame.getBasis()[2][1] *= btScalar(-1.);
	m_rbBFrame.getBasis()[2][2] *= btScalar(-1.);
	
	m_swingSpan1 = btScalar(1e30);
	m_swingSpan2 = btScalar(1e30);
	m_twistSpan  = btScalar(1e30);
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;

	m_solveTwistLimit = false;
	m_solveSwingLimit = false;
	
}			

void	btConeTwistConstraint::buildJacobian()
{
	m_appliedImpulse = btScalar(0.);

	//set bias, sign, clear accumulator
	m_swingCorrection = btScalar(0.);
	m_twistLimitSign = btScalar(0.);
	m_solveTwistLimit = false;
	m_solveSwingLimit = false;
	m_accTwistLimitImpulse = btScalar(0.);
	m_accSwingLimitImpulse = btScalar(0.);

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

	btVector3 b1Axis1,b1Axis2,b1Axis3;
	btVector3 b2Axis1,b2Axis2;

	b1Axis1 = getRigidBodyA().getCenterOfMassTransform().getBasis() * this->m_rbAFrame.getBasis().getColumn(0);
	b2Axis1 = getRigidBodyB().getCenterOfMassTransform().getBasis() * this->m_rbBFrame.getBasis().getColumn(0);

	btScalar swing1=btScalar(0.),swing2 = btScalar(0.);

	// Get Frame into world space
	if (m_swingSpan1 >= btScalar(0.05f))
	{
		b1Axis2 = getRigidBodyA().getCenterOfMassTransform().getBasis() * this->m_rbAFrame.getBasis().getColumn(1);
		swing1  = btAtan2Fast( b2Axis1.dot(b1Axis2),b2Axis1.dot(b1Axis1) );
	}

	if (m_swingSpan2 >= btScalar(0.05f))
	{
		b1Axis3 = getRigidBodyA().getCenterOfMassTransform().getBasis() * this->m_rbAFrame.getBasis().getColumn(2);			
		swing2 = btAtan2Fast( b2Axis1.dot(b1Axis3),b2Axis1.dot(b1Axis1) );
	}

	btScalar RMaxAngle1Sq = 1.0f / (m_swingSpan1*m_swingSpan1);		
	btScalar RMaxAngle2Sq = 1.0f / (m_swingSpan2*m_swingSpan2);	
	btScalar EllipseAngle = btFabs(swing1)* RMaxAngle1Sq + btFabs(swing2) * RMaxAngle2Sq;

	if (EllipseAngle > 1.0f)
	{
		m_swingCorrection = EllipseAngle-1.0f;
		m_solveSwingLimit = true;
		
		// Calculate necessary axis & factors
		m_swingAxis = b2Axis1.cross(b1Axis2* b2Axis1.dot(b1Axis2) + b1Axis3* b2Axis1.dot(b1Axis3));
		m_swingAxis.normalize();

		btScalar swingAxisSign = (b2Axis1.dot(b1Axis1) >= 0.0f) ? 1.0f : -1.0f;
		m_swingAxis *= swingAxisSign;

		m_kSwing =  btScalar(1.) / (getRigidBodyA().computeAngularImpulseDenominator(m_swingAxis) +
			getRigidBodyB().computeAngularImpulseDenominator(m_swingAxis));

	}

	// Twist limits
	if (m_twistSpan >= btScalar(0.))
	{
		btVector3 b2Axis2 = getRigidBodyB().getCenterOfMassTransform().getBasis() * this->m_rbBFrame.getBasis().getColumn(1);
		btQuaternion rotationArc = shortestArcQuat(b2Axis1,b1Axis1);
		btVector3 TwistRef = quatRotate(rotationArc,b2Axis2); 
		btScalar twist = btAtan2Fast( TwistRef.dot(b1Axis3), TwistRef.dot(b1Axis2) );

		btScalar lockedFreeFactor = (m_twistSpan > btScalar(0.05f)) ? m_limitSoftness : btScalar(0.);
		if (twist <= -m_twistSpan*lockedFreeFactor)
		{
			m_twistCorrection = -(twist + m_twistSpan);
			m_solveTwistLimit = true;

			m_twistAxis = (b2Axis1 + b1Axis1) * 0.5f;
			m_twistAxis.normalize();
			m_twistAxis *= -1.0f;

			m_kTwist = btScalar(1.) / (getRigidBodyA().computeAngularImpulseDenominator(m_twistAxis) +
				getRigidBodyB().computeAngularImpulseDenominator(m_twistAxis));

		}	else
			if (twist >  m_twistSpan*lockedFreeFactor)
			{
				m_twistCorrection = (twist - m_twistSpan);
				m_solveTwistLimit = true;

				m_twistAxis = (b2Axis1 + b1Axis1) * 0.5f;
				m_twistAxis.normalize();

				m_kTwist = btScalar(1.) / (getRigidBodyA().computeAngularImpulseDenominator(m_twistAxis) +
					getRigidBodyB().computeAngularImpulseDenominator(m_twistAxis));

			}
	}
}

void	btConeTwistConstraint::solveConstraint(btScalar	timeStep)
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
		const btVector3& angVelA = getRigidBodyA().getAngularVelocity();
		const btVector3& angVelB = getRigidBodyB().getAngularVelocity();

		// solve swing limit
		if (m_solveSwingLimit)
		{
			btScalar amplitude = ((angVelB - angVelA).dot( m_swingAxis )*m_relaxationFactor*m_relaxationFactor + m_swingCorrection*(btScalar(1.)/timeStep)*m_biasFactor);
			btScalar impulseMag = amplitude * m_kSwing;

			// Clamp the accumulated impulse
			btScalar temp = m_accSwingLimitImpulse;
			m_accSwingLimitImpulse = btMax(m_accSwingLimitImpulse + impulseMag, 0.0f );
			impulseMag = m_accSwingLimitImpulse - temp;

			btVector3 impulse = m_swingAxis * impulseMag;

			m_rbA.applyTorqueImpulse(impulse);
			m_rbB.applyTorqueImpulse(-impulse);

		}

		// solve twist limit
		if (m_solveTwistLimit)
		{
			btScalar amplitude = ((angVelB - angVelA).dot( m_twistAxis )*m_relaxationFactor*m_relaxationFactor + m_twistCorrection*(btScalar(1.)/timeStep)*m_biasFactor );
			btScalar impulseMag = amplitude * m_kTwist;

			// Clamp the accumulated impulse
			btScalar temp = m_accTwistLimitImpulse;
			m_accTwistLimitImpulse = btMax(m_accTwistLimitImpulse + impulseMag, 0.0f );
			impulseMag = m_accTwistLimitImpulse - temp;

			btVector3 impulse = m_twistAxis * impulseMag;

			m_rbA.applyTorqueImpulse(impulse);
			m_rbB.applyTorqueImpulse(-impulse);

		}
	
	}

}

void	btConeTwistConstraint::updateRHS(btScalar	timeStep)
{
	(void)timeStep;

}
