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



#include "btSliderConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <new>



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

}



btSliderConstraint::btSliderConstraint()
        :btTypedConstraint(SLIDER_CONSTRAINT_TYPE),
		m_useLinearReferenceFrameA(true),
		m_useSolveConstraintObsolete(false)
//		m_useSolveConstraintObsolete(true)
{
	initParams();
}



btSliderConstraint::btSliderConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB, bool useLinearReferenceFrameA)
        : btTypedConstraint(SLIDER_CONSTRAINT_TYPE, rbA, rbB)
        , m_frameInA(frameInA)
        , m_frameInB(frameInB),
		m_useLinearReferenceFrameA(useLinearReferenceFrameA),
		m_useSolveConstraintObsolete(false)
//		m_useSolveConstraintObsolete(true)
{
	initParams();
}


static btRigidBody s_fixed(0, 0, 0);
btSliderConstraint::btSliderConstraint(btRigidBody& rbB, const btTransform& frameInB, bool useLinearReferenceFrameB)
        : btTypedConstraint(SLIDER_CONSTRAINT_TYPE, s_fixed, rbB)
        ,
        m_frameInB(frameInB),
		m_useLinearReferenceFrameA(useLinearReferenceFrameB),
		m_useSolveConstraintObsolete(false)
//		m_useSolveConstraintObsolete(true)
{
	///not providing rigidbody B means implicitly using worldspace for body B
//	m_frameInA.getOrigin() = m_rbA.getCenterOfMassTransform()(m_frameInA.getOrigin());

	initParams();
}



void btSliderConstraint::buildJacobian()
{
	if (!m_useSolveConstraintObsolete) 
	{
		return;
	}
	if(m_useLinearReferenceFrameA)
	{
		buildJacobianInt(m_rbA, m_rbB, m_frameInA, m_frameInB);
	}
	else
	{
		buildJacobianInt(m_rbB, m_rbA, m_frameInB, m_frameInA);
	}
}



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
}



void btSliderConstraint::getInfo1(btConstraintInfo1* info)
{
	if (m_useSolveConstraintObsolete)
	{
		info->m_numConstraintRows = 0;
		info->nub = 0;
	}
	else
	{
		info->m_numConstraintRows = 4; // Fixed 2 linear + 2 angular
		info->nub = 2; 
		//prepare constraint
		calculateTransforms();
		testLinLimits();
		if(getSolveLinLimit() || getPoweredLinMotor())
		{
			info->m_numConstraintRows++; // limit 3rd linear as well
			info->nub--; 
		}
		testAngLimits();
		if(getSolveAngLimit() || getPoweredAngMotor())
		{
			info->m_numConstraintRows++; // limit 3rd angular as well
			info->nub--; 
		}
	}
}



void btSliderConstraint::getInfo2(btConstraintInfo2* info)
{
	btAssert(!m_useSolveConstraintObsolete);
	int i, s = info->rowskip;
	const btTransform& trA = getCalculatedTransformA();
	const btTransform& trB = getCalculatedTransformB();
	btScalar signFact = m_useLinearReferenceFrameA ? btScalar(1.0f) : btScalar(-1.0f);
	// make rotations around Y and Z equal
	// the slider axis should be the only unconstrained
	// rotational axis, the angular velocity of the two bodies perpendicular to
	// the slider axis should be equal. thus the constraint equations are
	//    p*w1 - p*w2 = 0
	//    q*w1 - q*w2 = 0
	// where p and q are unit vectors normal to the slider axis, and w1 and w2
	// are the angular velocity vectors of the two bodies.
	// get slider axis (X)
	btVector3 ax1 = trA.getBasis().getColumn(0);
	// get 2 orthos to slider axis (Y, Z)
	btVector3 p = trA.getBasis().getColumn(1);
	btVector3 q = trA.getBasis().getColumn(2);
	// set the two slider rows 
	info->m_J1angularAxis[0] = p[0];
	info->m_J1angularAxis[1] = p[1];
	info->m_J1angularAxis[2] = p[2];
	info->m_J1angularAxis[s+0] = q[0];
	info->m_J1angularAxis[s+1] = q[1];
	info->m_J1angularAxis[s+2] = q[2];

	info->m_J2angularAxis[0] = -p[0];
	info->m_J2angularAxis[1] = -p[1];
	info->m_J2angularAxis[2] = -p[2];
	info->m_J2angularAxis[s+0] = -q[0];
	info->m_J2angularAxis[s+1] = -q[1];
	info->m_J2angularAxis[s+2] = -q[2];
	// compute the right hand side of the constraint equation. set relative
	// body velocities along p and q to bring the slider back into alignment.
	// if ax1,ax2 are the unit length slider axes as computed from body1 and
	// body2, we need to rotate both bodies along the axis u = (ax1 x ax2).
	// if "theta" is the angle between ax1 and ax2, we need an angular velocity
	// along u to cover angle erp*theta in one step :
	//   |angular_velocity| = angle/time = erp*theta / stepsize
	//                      = (erp*fps) * theta
	//    angular_velocity  = |angular_velocity| * (ax1 x ax2) / |ax1 x ax2|
	//                      = (erp*fps) * theta * (ax1 x ax2) / sin(theta)
	// ...as ax1 and ax2 are unit length. if theta is smallish,
	// theta ~= sin(theta), so
	//    angular_velocity  = (erp*fps) * (ax1 x ax2)
	// ax1 x ax2 is in the plane space of ax1, so we project the angular
	// velocity to p and q to find the right hand side.
	btScalar k = info->fps * info->erp * getSoftnessOrthoAng();
    btVector3 ax2 = trB.getBasis().getColumn(0);
	btVector3 u = ax1.cross(ax2);
	info->m_constraintError[0] = k * u.dot(p);
	info->m_constraintError[s] = k * u.dot(q);
	// pull out pos and R for both bodies. also get the connection
	// vector c = pos2-pos1.
	// next two rows. we want: vel2 = vel1 + w1 x c ... but this would
	// result in three equations, so we project along the planespace vectors
	// so that sliding along the slider axis is disregarded. for symmetry we
	// also consider rotation around center of mass of two bodies (factA and factB).
	btTransform bodyA_trans = m_rbA.getCenterOfMassTransform();
	btTransform bodyB_trans = m_rbB.getCenterOfMassTransform();
	int s2 = 2 * s, s3 = 3 * s;
	btVector3 c;
	btScalar miA = m_rbA.getInvMass();
	btScalar miB = m_rbB.getInvMass();
	btScalar miS = miA + miB;
	btScalar factA, factB;
	if(miS > btScalar(0.f))
	{
		factA = miB / miS;
	}
	else 
	{
		factA = btScalar(0.5f);
	}
	if(factA > 0.99f) factA = 0.99f;
	if(factA < 0.01f) factA = 0.01f;
	factB = btScalar(1.0f) - factA;
	c = bodyB_trans.getOrigin() - bodyA_trans.getOrigin();
	btVector3 tmp = c.cross(p);
	for (i=0; i<3; i++) info->m_J1angularAxis[s2+i] = factA*tmp[i];
	for (i=0; i<3; i++) info->m_J2angularAxis[s2+i] = factB*tmp[i];
	tmp = c.cross(q);
	for (i=0; i<3; i++) info->m_J1angularAxis[s3+i] = factA*tmp[i];
	for (i=0; i<3; i++) info->m_J2angularAxis[s3+i] = factB*tmp[i];

	for (i=0; i<3; i++) info->m_J1linearAxis[s2+i] = p[i];
	for (i=0; i<3; i++) info->m_J1linearAxis[s3+i] = q[i];
	// compute two elements of right hand side. we want to align the offset
	// point (in body 2's frame) with the center of body 1.
	btVector3 ofs; // offset point in global coordinates
	ofs = trB.getOrigin() - trA.getOrigin();
	k = info->fps * info->erp * getSoftnessOrthoLin();
	info->m_constraintError[s2] = k * p.dot(ofs);
	info->m_constraintError[s3] = k * q.dot(ofs);
	int nrow = 3; // last filled row
	int srow;
	// check linear limits linear
	btScalar limit_err = btScalar(0.0);
	int limit = 0;
	if(getSolveLinLimit())
	{
		limit_err = getLinDepth() *  signFact;
		limit = (limit_err > btScalar(0.0)) ? 2 : 1;
	}
	int powered = 0;
	if(getPoweredLinMotor())
	{
		powered = 1;
	}
	// if the slider has joint limits or motor, add in the extra row
	if (limit || powered) 
	{
		nrow++;
		srow = nrow * info->rowskip;
		info->m_J1linearAxis[srow+0] = ax1[0];
		info->m_J1linearAxis[srow+1] = ax1[1];
		info->m_J1linearAxis[srow+2] = ax1[2];
		// linear torque decoupling step:
		//
		// we have to be careful that the linear constraint forces (+/- ax1) applied to the two bodies
		// do not create a torque couple. in other words, the points that the
		// constraint force is applied at must lie along the same ax1 axis.
		// a torque couple will result in limited slider-jointed free
		// bodies from gaining angular momentum.
		// the solution used here is to apply the constraint forces at the center of mass of the two bodies
		btVector3 ltd;	// Linear Torque Decoupling vector (a torque)
//		c = btScalar(0.5) * c;
		ltd = c.cross(ax1);
		info->m_J1angularAxis[srow+0] = factA*ltd[0];
		info->m_J1angularAxis[srow+1] = factA*ltd[1];
		info->m_J1angularAxis[srow+2] = factA*ltd[2];
		info->m_J2angularAxis[srow+0] = factB*ltd[0];
		info->m_J2angularAxis[srow+1] = factB*ltd[1];
		info->m_J2angularAxis[srow+2] = factB*ltd[2];
		// right-hand part
		btScalar lostop = getLowerLinLimit();
		btScalar histop = getUpperLinLimit();
		if(limit && (lostop == histop))
		{  // the joint motor is ineffective
			powered = 0;
		}
		info->m_constraintError[srow] = 0.;
		info->m_lowerLimit[srow] = 0.;
		info->m_upperLimit[srow] = 0.;
		if(powered)
		{
            info->cfm[nrow] = btScalar(0.0); 
			btScalar tag_vel = getTargetLinMotorVelocity();
			btScalar mot_fact = getMotorFactor(m_linPos, m_lowerLinLimit, m_upperLinLimit, tag_vel, info->fps * info->erp);
//			info->m_constraintError[srow] += mot_fact * getTargetLinMotorVelocity();
			info->m_constraintError[srow] -= signFact * mot_fact * getTargetLinMotorVelocity();
			info->m_lowerLimit[srow] += -getMaxLinMotorForce() * info->fps;
			info->m_upperLimit[srow] += getMaxLinMotorForce() * info->fps;
		}
		if(limit)
		{
			k = info->fps * info->erp;
			info->m_constraintError[srow] += k * limit_err;
			info->cfm[srow] = btScalar(0.0); // stop_cfm;
			if(lostop == histop) 
			{	// limited low and high simultaneously
				info->m_lowerLimit[srow] = -SIMD_INFINITY;
				info->m_upperLimit[srow] = SIMD_INFINITY;
			}
			else if(limit == 1) 
			{ // low limit
				info->m_lowerLimit[srow] = -SIMD_INFINITY;
				info->m_upperLimit[srow] = 0;
			}
			else 
			{ // high limit
				info->m_lowerLimit[srow] = 0;
				info->m_upperLimit[srow] = SIMD_INFINITY;
			}
			// bounce (we'll use slider parameter abs(1.0 - m_dampingLimLin) for that)
			btScalar bounce = btFabs(btScalar(1.0) - getDampingLimLin());
			if(bounce > btScalar(0.0))
			{
				btScalar vel = m_rbA.getLinearVelocity().dot(ax1);
				vel -= m_rbB.getLinearVelocity().dot(ax1);
				vel *= signFact;
				// only apply bounce if the velocity is incoming, and if the
				// resulting c[] exceeds what we already have.
				if(limit == 1)
				{	// low limit
					if(vel < 0)
					{
						btScalar newc = -bounce * vel;
						if (newc > info->m_constraintError[srow])
						{
							info->m_constraintError[srow] = newc;
						}
					}
				}
				else
				{ // high limit - all those computations are reversed
					if(vel > 0)
					{
						btScalar newc = -bounce * vel;
						if(newc < info->m_constraintError[srow]) 
						{
							info->m_constraintError[srow] = newc;
						}
					}
				}
			}
			info->m_constraintError[srow] *= getSoftnessLimLin();
		} // if(limit)
	} // if linear limit
	// check angular limits
	limit_err = btScalar(0.0);
	limit = 0;
	if(getSolveAngLimit())
	{
		limit_err = getAngDepth();
		limit = (limit_err > btScalar(0.0)) ? 1 : 2;
	}
	// if the slider has joint limits, add in the extra row
	powered = 0;
	if(getPoweredAngMotor())
	{
		powered = 1;
	}
	if(limit || powered) 
	{
		nrow++;
		srow = nrow * info->rowskip;
		info->m_J1angularAxis[srow+0] = ax1[0];
		info->m_J1angularAxis[srow+1] = ax1[1];
		info->m_J1angularAxis[srow+2] = ax1[2];

		info->m_J2angularAxis[srow+0] = -ax1[0];
		info->m_J2angularAxis[srow+1] = -ax1[1];
		info->m_J2angularAxis[srow+2] = -ax1[2];

		btScalar lostop = getLowerAngLimit();
		btScalar histop = getUpperAngLimit();
		if(limit && (lostop == histop))
		{  // the joint motor is ineffective
			powered = 0;
		}
		if(powered)
		{
            info->cfm[srow] = btScalar(0.0); 
			btScalar mot_fact = getMotorFactor(m_angPos, m_lowerAngLimit, m_upperAngLimit, getTargetAngMotorVelocity(), info->fps * info->erp);
			info->m_constraintError[srow] = mot_fact * getTargetAngMotorVelocity();
			info->m_lowerLimit[srow] = -getMaxAngMotorForce() * info->fps;
			info->m_upperLimit[srow] = getMaxAngMotorForce() * info->fps;
		}
		if(limit)
		{
			k = info->fps * info->erp;
			info->m_constraintError[srow] += k * limit_err;
			info->cfm[srow] = btScalar(0.0); // stop_cfm;
			if(lostop == histop) 
			{
				// limited low and high simultaneously
				info->m_lowerLimit[srow] = -SIMD_INFINITY;
				info->m_upperLimit[srow] = SIMD_INFINITY;
			}
			else if(limit == 1) 
			{ // low limit
				info->m_lowerLimit[srow] = 0;
				info->m_upperLimit[srow] = SIMD_INFINITY;
			}
			else 
			{ // high limit
				info->m_lowerLimit[srow] = -SIMD_INFINITY;
				info->m_upperLimit[srow] = 0;
			}
			// bounce (we'll use slider parameter abs(1.0 - m_dampingLimAng) for that)
			btScalar bounce = btFabs(btScalar(1.0) - getDampingLimAng());
			if(bounce > btScalar(0.0))
			{
				btScalar vel = m_rbA.getAngularVelocity().dot(ax1);
				vel -= m_rbB.getAngularVelocity().dot(ax1);
				// only apply bounce if the velocity is incoming, and if the
				// resulting c[] exceeds what we already have.
				if(limit == 1)
				{	// low limit
					if(vel < 0)
					{
						btScalar newc = -bounce * vel;
						if(newc > info->m_constraintError[srow])
						{
							info->m_constraintError[srow] = newc;
						}
					}
				}
				else
				{	// high limit - all those computations are reversed
					if(vel > 0)
					{
						btScalar newc = -bounce * vel;
						if(newc < info->m_constraintError[srow])
						{
							info->m_constraintError[srow] = newc;
						}
					}
				}
			}
			info->m_constraintError[srow] *= getSoftnessLimAng();
		} // if(limit)
	} // if angular limit or powered
}



void btSliderConstraint::solveConstraintObsolete(btSolverBody& bodyA,btSolverBody& bodyB,btScalar timeStep)
{
	if (m_useSolveConstraintObsolete)
	{
		m_timeStep = timeStep;
		if(m_useLinearReferenceFrameA)
		{
			solveConstraintInt(m_rbA,bodyA, m_rbB,bodyB);
		}
		else
		{
			solveConstraintInt(m_rbB,bodyB, m_rbA,bodyA);
		}
	}
}



void btSliderConstraint::solveConstraintInt(btRigidBody& rbA, btSolverBody& bodyA,btRigidBody& rbB, btSolverBody& bodyB)
{
    int i;
    // linear
    btVector3 velA;
	bodyA.getVelocityInLocalPointObsolete(m_relPosA,velA);
    btVector3 velB;
	bodyB.getVelocityInLocalPointObsolete(m_relPosB,velB);
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
		
		//rbA.applyImpulse( impulse_vector, m_relPosA);
		//rbB.applyImpulse(-impulse_vector, m_relPosB);
		{
			btVector3 ftorqueAxis1 = m_relPosA.cross(normal);
			btVector3 ftorqueAxis2 = m_relPosB.cross(normal);
			bodyA.applyImpulse(normal*rbA.getInvMass(), rbA.getInvInertiaTensorWorld()*ftorqueAxis1,normalImpulse);
			bodyB.applyImpulse(normal*rbB.getInvMass(), rbB.getInvInertiaTensorWorld()*ftorqueAxis2,-normalImpulse);
		}



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
				//rbA.applyImpulse( impulse_vector, m_relPosA);
				//rbB.applyImpulse(-impulse_vector, m_relPosB);

				{
					btVector3 ftorqueAxis1 = m_relPosA.cross(normal);
					btVector3 ftorqueAxis2 = m_relPosB.cross(normal);
					bodyA.applyImpulse(normal*rbA.getInvMass(), rbA.getInvInertiaTensorWorld()*ftorqueAxis1,normalImpulse);
					bodyB.applyImpulse(normal*rbB.getInvMass(), rbB.getInvInertiaTensorWorld()*ftorqueAxis2,-normalImpulse);
				}



			}
		}
    }
	// angular 
	// get axes in world space
	btVector3 axisA =  m_calculatedTransformA.getBasis().getColumn(0);
	btVector3 axisB =  m_calculatedTransformB.getBasis().getColumn(0);

	btVector3 angVelA;
	bodyA.getAngularVelocity(angVelA);
	btVector3 angVelB;
	bodyB.getAngularVelocity(angVelB);

	btVector3 angVelAroundAxisA = axisA * axisA.dot(angVelA);
	btVector3 angVelAroundAxisB = axisB * axisB.dot(angVelB);

	btVector3 angAorthog = angVelA - angVelAroundAxisA;
	btVector3 angBorthog = angVelB - angVelAroundAxisB;
	btVector3 velrelOrthog = angAorthog-angBorthog;
	//solve orthogonal angular velocity correction
	btScalar len = velrelOrthog.length();
	btScalar orthorImpulseMag = 0.f;

	if (len > btScalar(0.00001))
	{
		btVector3 normal = velrelOrthog.normalized();
		btScalar denom = rbA.computeAngularImpulseDenominator(normal) + rbB.computeAngularImpulseDenominator(normal);
		//velrelOrthog *= (btScalar(1.)/denom) * m_dampingOrthoAng * m_softnessOrthoAng;
		orthorImpulseMag = (btScalar(1.)/denom) * m_dampingOrthoAng * m_softnessOrthoAng;
	}
	//solve angular positional correction
	btVector3 angularError = axisA.cross(axisB) *(btScalar(1.)/m_timeStep);
	btVector3 angularAxis = angularError;
	btScalar angularImpulseMag = 0;

	btScalar len2 = angularError.length();
	if (len2>btScalar(0.00001))
	{
		btVector3 normal2 = angularError.normalized();
		btScalar denom2 = rbA.computeAngularImpulseDenominator(normal2) + rbB.computeAngularImpulseDenominator(normal2);
		angularImpulseMag = (btScalar(1.)/denom2) * m_restitutionOrthoAng * m_softnessOrthoAng;
		angularError *= angularImpulseMag;
	}
	// apply impulse
	//rbA.applyTorqueImpulse(-velrelOrthog+angularError);
	//rbB.applyTorqueImpulse(velrelOrthog-angularError);

	bodyA.applyImpulse(btVector3(0,0,0), rbA.getInvInertiaTensorWorld()*velrelOrthog,-orthorImpulseMag);
	bodyB.applyImpulse(btVector3(0,0,0), rbB.getInvInertiaTensorWorld()*velrelOrthog,orthorImpulseMag);
	bodyA.applyImpulse(btVector3(0,0,0), rbA.getInvInertiaTensorWorld()*angularAxis,angularImpulseMag);
	bodyB.applyImpulse(btVector3(0,0,0), rbB.getInvInertiaTensorWorld()*angularAxis,-angularImpulseMag);


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
	//rbA.applyTorqueImpulse(impulse);
	//rbB.applyTorqueImpulse(-impulse);

	bodyA.applyImpulse(btVector3(0,0,0), rbA.getInvInertiaTensorWorld()*axisA,impulseMag);
	bodyB.applyImpulse(btVector3(0,0,0), rbB.getInvInertiaTensorWorld()*axisA,-impulseMag);



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
			//rbA.applyTorqueImpulse(motorImp);
			//rbB.applyTorqueImpulse(-motorImp);

			bodyA.applyImpulse(btVector3(0,0,0), rbA.getInvInertiaTensorWorld()*axisA,angImpulse);
			bodyB.applyImpulse(btVector3(0,0,0), rbB.getInvInertiaTensorWorld()*axisA,-angImpulse);
		}
	}
}





void btSliderConstraint::calculateTransforms(void){
	if(m_useLinearReferenceFrameA || (!m_useSolveConstraintObsolete))
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
	if(m_useLinearReferenceFrameA || m_useSolveConstraintObsolete)
	{
		m_delta = m_realPivotBInW - m_realPivotAInW;
	}
	else
	{
		m_delta = m_realPivotAInW - m_realPivotBInW;
	}
	m_projPivotInW = m_realPivotAInW + m_sliderAxis.dot(m_delta) * m_sliderAxis;
    btVector3 normalWorld;
    int i;
    //linear part
    for(i = 0; i < 3; i++)
    {
		normalWorld = m_calculatedTransformA.getBasis().getColumn(i);
		m_depth[i] = m_delta.dot(normalWorld);
    }
}
 


void btSliderConstraint::testLinLimits(void)
{
	m_solveLinLim = false;
	m_linPos = m_depth[0];
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
}



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
		m_angPos = rot;
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
}
	


btVector3 btSliderConstraint::getAncorInA(void)
{
	btVector3 ancorInA;
	ancorInA = m_realPivotAInW + (m_lowerLinLimit + m_upperLinLimit) * btScalar(0.5) * m_sliderAxis;
	ancorInA = m_rbA.getCenterOfMassTransform().inverse() * ancorInA;
	return ancorInA;
}



btVector3 btSliderConstraint::getAncorInB(void)
{
	btVector3 ancorInB;
	ancorInB = m_frameInB.getOrigin();
	return ancorInB;
}
