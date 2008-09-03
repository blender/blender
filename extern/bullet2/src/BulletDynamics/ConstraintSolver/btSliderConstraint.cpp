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

/*
Added by Roman Ponomarev (rponom@gmail.com)
April 04, 2008
*/

//-----------------------------------------------------------------------------

#include "btSliderConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <new>

//-----------------------------------------------------------------------------

void btSliderConstraint::initParams()
{
    m_lowerLinLimit = btScalar(1.0);
    m_upperLinLimit = btScalar(-1.0);
    m_lowerAngLimit = btScalar(0.);
    m_upperAngLimit = btScalar(0.);
	m_softnessDirLin = SLIDER_CONSTRAINT_DEF_SOFTNESS;
	m_restitutionDirLin = SLIDER_CONSTRAINT_DEF_RESTITUTION;
	m_dampingDirLin = btScalar(0.);
	m_softnessDirAng = SLIDER_CONSTRAINT_DEF_SOFTNESS;
	m_restitutionDirAng = SLIDER_CONSTRAINT_DEF_RESTITUTION;
	m_dampingDirAng = btScalar(0.);
	m_softnessOrthoLin = SLIDER_CONSTRAINT_DEF_SOFTNESS;
	m_restitutionOrthoLin = SLIDER_CONSTRAINT_DEF_RESTITUTION;
	m_dampingOrthoLin = SLIDER_CONSTRAINT_DEF_DAMPING;
	m_softnessOrthoAng = SLIDER_CONSTRAINT_DEF_SOFTNESS;
	m_restitutionOrthoAng = SLIDER_CONSTRAINT_DEF_RESTITUTION;
	m_dampingOrthoAng = SLIDER_CONSTRAINT_DEF_DAMPING;
	m_softnessLimLin = SLIDER_CONSTRAINT_DEF_SOFTNESS;
	m_restitutionLimLin = SLIDER_CONSTRAINT_DEF_RESTITUTION;
	m_dampingLimLin = SLIDER_CONSTRAINT_DEF_DAMPING;
	m_softnessLimAng = SLIDER_CONSTRAINT_DEF_SOFTNESS;
	m_restitutionLimAng = SLIDER_CONSTRAINT_DEF_RESTITUTION;
	m_dampingLimAng = SLIDER_CONSTRAINT_DEF_DAMPING;

	m_poweredLinMotor = false;
    m_targetLinMotorVelocity = btScalar(0.);
    m_maxLinMotorForce = btScalar(0.);
	m_accumulatedLinMotorImpulse = btScalar(0.0);

	m_poweredAngMotor = false;
    m_targetAngMotorVelocity = btScalar(0.);
    m_maxAngMotorForce = btScalar(0.);
	m_accumulatedAngMotorImpulse = btScalar(0.0);

} // btSliderConstraint::initParams()

//-----------------------------------------------------------------------------

btSliderConstraint::btSliderConstraint()
        :btTypedConstraint(SLIDER_CONSTRAINT_TYPE),
		m_useLinearReferenceFrameA(true)
{
	initParams();
} // btSliderConstraint::btSliderConstraint()

//-----------------------------------------------------------------------------

btSliderConstraint::btSliderConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB, bool useLinearReferenceFrameA)
        : btTypedConstraint(SLIDER_CONSTRAINT_TYPE, rbA, rbB)
        , m_frameInA(frameInA)
        , m_frameInB(frameInB),
		m_useLinearReferenceFrameA(useLinearReferenceFrameA)
{
	initParams();
} // btSliderConstraint::btSliderConstraint()

//-----------------------------------------------------------------------------

void btSliderConstraint::buildJacobian()
{
	if(m_useLinearReferenceFrameA)
	{
		buildJacobianInt(m_rbA, m_rbB, m_frameInA, m_frameInB);
	}
	else
	{
		buildJacobianInt(m_rbB, m_rbA, m_frameInB, m_frameInA);
	}
} // btSliderConstraint::buildJacobian()

//-----------------------------------------------------------------------------

void btSliderConstraint::buildJacobianInt(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB)
{
	//calculate transforms
    m_calculatedTransformA = rbA.getCenterOfMassTransform() * frameInA;
    m_calculatedTransformB = rbB.getCenterOfMassTransform() * frameInB;
	m_realPivotAInW = m_calculatedTransformA.getOrigin();
	m_realPivotBInW = m_calculatedTransformB.getOrigin();
	m_sliderAxis = m_calculatedTransformA.getBasis().getColumn(0); // along X
	m_delta = m_realPivotBInW - m_realPivotAInW;
	m_projPivotInW = m_realPivotAInW + m_sliderAxis.dot(m_delta) * m_sliderAxis;
	m_relPosA = m_projPivotInW - rbA.getCenterOfMassPosition();
	m_relPosB = m_realPivotBInW - rbB.getCenterOfMassPosition();
    btVector3 normalWorld;
    int i;
    //linear part
    for(i = 0; i < 3; i++)
    {
		normalWorld = m_calculatedTransformA.getBasis().getColumn(i);
		new (&m_jacLin[i]) btJacobianEntry(
			rbA.getCenterOfMassTransform().getBasis().transpose(),
			rbB.getCenterOfMassTransform().getBasis().transpose(),
			m_relPosA,
			m_relPosB,
			normalWorld,
			rbA.getInvInertiaDiagLocal(),
			rbA.getInvMass(),
			rbB.getInvInertiaDiagLocal(),
			rbB.getInvMass()
			);
		m_jacLinDiagABInv[i] = btScalar(1.) / m_jacLin[i].getDiagonal();
		m_depth[i] = m_delta.dot(normalWorld);
    }
	testLinLimits();
    // angular part
    for(i = 0; i < 3; i++)
    {
		normalWorld = m_calculatedTransformA.getBasis().getColumn(i);
		new (&m_jacAng[i])	btJacobianEntry(
			normalWorld,
            rbA.getCenterOfMassTransform().getBasis().transpose(),
            rbB.getCenterOfMassTransform().getBasis().transpose(),
            rbA.getInvInertiaDiagLocal(),
            rbB.getInvInertiaDiagLocal()
			);
	}
	testAngLimits();
	btVector3 axisA = m_calculatedTransformA.getBasis().getColumn(0);
	m_kAngle = btScalar(1.0 )/ (rbA.computeAngularImpulseDenominator(axisA) + rbB.computeAngularImpulseDenominator(axisA));
	// clear accumulator for motors
	m_accumulatedLinMotorImpulse = btScalar(0.0);
	m_accumulatedAngMotorImpulse = btScalar(0.0);
} // btSliderConstraint::buildJacobianInt()

//-----------------------------------------------------------------------------

void btSliderConstraint::solveConstraint(btScalar timeStep)
{
    m_timeStep = timeStep;
	if(m_useLinearReferenceFrameA)
	{
		solveConstraintInt(m_rbA, m_rbB);
	}
	else
	{
		solveConstraintInt(m_rbB, m_rbA);
	}
} // btSliderConstraint::solveConstraint()

//-----------------------------------------------------------------------------

void btSliderConstraint::solveConstraintInt(btRigidBody& rbA, btRigidBody& rbB)
{
    int i;
    // linear
    btVector3 velA = rbA.getVelocityInLocalPoint(m_relPosA);
    btVector3 velB = rbB.getVelocityInLocalPoint(m_relPosB);
    btVector3 vel = velA - velB;
	for(i = 0; i < 3; i++)
    {
		const btVector3& normal = m_jacLin[i].m_linearJointAxis;
		btScalar rel_vel = normal.dot(vel);
		// calculate positional error
		btScalar depth = m_depth[i];
		// get parameters
		btScalar softness = (i) ? m_softnessOrthoLin : (m_solveLinLim ? m_softnessLimLin : m_softnessDirLin);
		btScalar restitution = (i) ? m_restitutionOrthoLin : (m_solveLinLim ? m_restitutionLimLin : m_restitutionDirLin);
		btScalar damping = (i) ? m_dampingOrthoLin : (m_solveLinLim ? m_dampingLimLin : m_dampingDirLin);
		// calcutate and apply impulse
		btScalar normalImpulse = softness * (restitution * depth / m_timeStep - damping * rel_vel) * m_jacLinDiagABInv[i];
		btVector3 impulse_vector = normal * normalImpulse;
		rbA.applyImpulse( impulse_vector, m_relPosA);
		rbB.applyImpulse(-impulse_vector, m_relPosB);
		if(m_poweredLinMotor && (!i))
		{ // apply linear motor
			if(m_accumulatedLinMotorImpulse < m_maxLinMotorForce)
			{
				btScalar desiredMotorVel = m_targetLinMotorVelocity;
				btScalar motor_relvel = desiredMotorVel + rel_vel;
				normalImpulse = -motor_relvel * m_jacLinDiagABInv[i];
				// clamp accumulated impulse
				btScalar new_acc = m_accumulatedLinMotorImpulse + btFabs(normalImpulse);
				if(new_acc  > m_maxLinMotorForce)
				{
					new_acc = m_maxLinMotorForce;
				}
				btScalar del = new_acc  - m_accumulatedLinMotorImpulse;
				if(normalImpulse < btScalar(0.0))
				{
					normalImpulse = -del;
				}
				else
				{
					normalImpulse = del;
				}
				m_accumulatedLinMotorImpulse = new_acc;
				// apply clamped impulse
				impulse_vector = normal * normalImpulse;
				rbA.applyImpulse( impulse_vector, m_relPosA);
				rbB.applyImpulse(-impulse_vector, m_relPosB);
			}
		}
    }
	// angular 
	// get axes in world space
	btVector3 axisA =  m_calculatedTransformA.getBasis().getColumn(0);
	btVector3 axisB =  m_calculatedTransformB.getBasis().getColumn(0);

	const btVector3& angVelA = rbA.getAngularVelocity();
	const btVector3& angVelB = rbB.getAngularVelocity();

	btVector3 angVelAroundAxisA = axisA * axisA.dot(angVelA);
	btVector3 angVelAroundAxisB = axisB * axisB.dot(angVelB);

	btVector3 angAorthog = angVelA - angVelAroundAxisA;
	btVector3 angBorthog = angVelB - angVelAroundAxisB;
	btVector3 velrelOrthog = angAorthog-angBorthog;
	//solve orthogonal angular velocity correction
	btScalar len = velrelOrthog.length();
	if (len > btScalar(0.00001))
	{
		btVector3 normal = velrelOrthog.normalized();
		btScalar denom = rbA.computeAngularImpulseDenominator(normal) + rbB.computeAngularImpulseDenominator(normal);
		velrelOrthog *= (btScalar(1.)/denom) * m_dampingOrthoAng * m_softnessOrthoAng;
	}
	//solve angular positional correction
	btVector3 angularError = axisA.cross(axisB) *(btScalar(1.)/m_timeStep);
	btScalar len2 = angularError.length();
	if (len2>btScalar(0.00001))
	{
		btVector3 normal2 = angularError.normalized();
		btScalar denom2 = rbA.computeAngularImpulseDenominator(normal2) + rbB.computeAngularImpulseDenominator(normal2);
		angularError *= (btScalar(1.)/denom2) * m_restitutionOrthoAng * m_softnessOrthoAng;
	}
	// apply impulse
	rbA.applyTorqueImpulse(-velrelOrthog+angularError);
	rbB.applyTorqueImpulse(velrelOrthog-angularError);
	btScalar impulseMag;
	//solve angular limits
	if(m_solveAngLim)
	{
		impulseMag = (angVelB - angVelA).dot(axisA) * m_dampingLimAng + m_angDepth * m_restitutionLimAng / m_timeStep;
		impulseMag *= m_kAngle * m_softnessLimAng;
	}
	else
	{
		impulseMag = (angVelB - angVelA).dot(axisA) * m_dampingDirAng + m_angDepth * m_restitutionDirAng / m_timeStep;
		impulseMag *= m_kAngle * m_softnessDirAng;
	}
	btVector3 impulse = axisA * impulseMag;
	rbA.applyTorqueImpulse(impulse);
	rbB.applyTorqueImpulse(-impulse);
	//apply angular motor
	if(m_poweredAngMotor) 
	{
		if(m_accumulatedAngMotorImpulse < m_maxAngMotorForce)
		{
			btVector3 velrel = angVelAroundAxisA - angVelAroundAxisB;
			btScalar projRelVel = velrel.dot(axisA);

			btScalar desiredMotorVel = m_targetAngMotorVelocity;
			btScalar motor_relvel = desiredMotorVel - projRelVel;

			btScalar angImpulse = m_kAngle * motor_relvel;
			// clamp accumulated impulse
			btScalar new_acc = m_accumulatedAngMotorImpulse + btFabs(angImpulse);
			if(new_acc  > m_maxAngMotorForce)
			{
				new_acc = m_maxAngMotorForce;
			}
			btScalar del = new_acc  - m_accumulatedAngMotorImpulse;
			if(angImpulse < btScalar(0.0))
			{
				angImpulse = -del;
			}
			else
			{
				angImpulse = del;
			}
			m_accumulatedAngMotorImpulse = new_acc;
			// apply clamped impulse
			btVector3 motorImp = angImpulse * axisA;
			m_rbA.applyTorqueImpulse(motorImp);
			m_rbB.applyTorqueImpulse(-motorImp);
		}
	}
} // btSliderConstraint::solveConstraint()

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void btSliderConstraint::calculateTransforms(void){
	if(m_useLinearReferenceFrameA)
	{
		m_calculatedTransformA = m_rbA.getCenterOfMassTransform() * m_frameInA;
		m_calculatedTransformB = m_rbB.getCenterOfMassTransform() * m_frameInB;
	}
	else
	{
		m_calculatedTransformA = m_rbB.getCenterOfMassTransform() * m_frameInB;
		m_calculatedTransformB = m_rbA.getCenterOfMassTransform() * m_frameInA;
	}
	m_realPivotAInW = m_calculatedTransformA.getOrigin();
	m_realPivotBInW = m_calculatedTransformB.getOrigin();
	m_sliderAxis = m_calculatedTransformA.getBasis().getColumn(0); // along X
	m_delta = m_realPivotBInW - m_realPivotAInW;
	m_projPivotInW = m_realPivotAInW + m_sliderAxis.dot(m_delta) * m_sliderAxis;
    btVector3 normalWorld;
    int i;
    //linear part
    for(i = 0; i < 3; i++)
    {
		normalWorld = m_calculatedTransformA.getBasis().getColumn(i);
		m_depth[i] = m_delta.dot(normalWorld);
    }
} // btSliderConstraint::calculateTransforms()
 
//-----------------------------------------------------------------------------

void btSliderConstraint::testLinLimits(void)
{
	m_solveLinLim = false;
	if(m_lowerLinLimit <= m_upperLinLimit)
	{
		if(m_depth[0] > m_upperLinLimit)
		{
			m_depth[0] -= m_upperLinLimit;
			m_solveLinLim = true;
		}
		else if(m_depth[0] < m_lowerLinLimit)
		{
			m_depth[0] -= m_lowerLinLimit;
			m_solveLinLim = true;
		}
		else
		{
			m_depth[0] = btScalar(0.);
		}
	}
	else
	{
		m_depth[0] = btScalar(0.);
	}
} // btSliderConstraint::testLinLimits()

//-----------------------------------------------------------------------------
 

void btSliderConstraint::testAngLimits(void)
{
	m_angDepth = btScalar(0.);
	m_solveAngLim = false;
	if(m_lowerAngLimit <= m_upperAngLimit)
	{
		const btVector3 axisA0 = m_calculatedTransformA.getBasis().getColumn(1);
		const btVector3 axisA1 = m_calculatedTransformA.getBasis().getColumn(2);
		const btVector3 axisB0 = m_calculatedTransformB.getBasis().getColumn(1);
		btScalar rot = btAtan2Fast(axisB0.dot(axisA1), axisB0.dot(axisA0));  
		if(rot < m_lowerAngLimit)
		{
			m_angDepth = rot - m_lowerAngLimit;
			m_solveAngLim = true;
		} 
		else if(rot > m_upperAngLimit)
		{
			m_angDepth = rot - m_upperAngLimit;
			m_solveAngLim = true;
		}
	}
} // btSliderConstraint::testAngLimits()

	
//-----------------------------------------------------------------------------



btVector3 btSliderConstraint::getAncorInA(void)
{
	btVector3 ancorInA;
	ancorInA = m_realPivotAInW + (m_lowerLinLimit + m_upperLinLimit) * btScalar(0.5) * m_sliderAxis;
	ancorInA = m_rbA.getCenterOfMassTransform().inverse() * ancorInA;
	return ancorInA;
} // btSliderConstraint::getAncorInA()

//-----------------------------------------------------------------------------

btVector3 btSliderConstraint::getAncorInB(void)
{
	btVector3 ancorInB;
	ancorInB = m_frameInB.getOrigin();
	return ancorInB;
} // btSliderConstraint::getAncorInB();
