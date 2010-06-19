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
#include "LinearMath/btMinMax.h"
#include <new>
#include "btSolverBody.h"

//-----------------------------------------------------------------------------

#define HINGE_USE_OBSOLETE_SOLVER false

//-----------------------------------------------------------------------------


btHingeConstraint::btHingeConstraint()
: btTypedConstraint (HINGE_CONSTRAINT_TYPE),
m_enableAngularMotor(false),
m_useSolveConstraintObsolete(HINGE_USE_OBSOLETE_SOLVER),
m_useReferenceFrameA(false)
{
	m_referenceSign = m_useReferenceFrameA ? btScalar(-1.f) : btScalar(1.f);
}

//-----------------------------------------------------------------------------

btHingeConstraint::btHingeConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB,
									 btVector3& axisInA,btVector3& axisInB, bool useReferenceFrameA)
									 :btTypedConstraint(HINGE_CONSTRAINT_TYPE, rbA,rbB),
									 m_angularOnly(false),
									 m_enableAngularMotor(false),
									 m_useSolveConstraintObsolete(HINGE_USE_OBSOLETE_SOLVER),
									 m_useReferenceFrameA(useReferenceFrameA)
{
	m_rbAFrame.getOrigin() = pivotInA;
	
	// since no frame is given, assume this to be zero angle and just pick rb transform axis
	btVector3 rbAxisA1 = rbA.getCenterOfMassTransform().getBasis().getColumn(0);

	btVector3 rbAxisA2;
	btScalar projection = axisInA.dot(rbAxisA1);
	if (projection >= 1.0f - SIMD_EPSILON) {
		rbAxisA1 = -rbA.getCenterOfMassTransform().getBasis().getColumn(2);
		rbAxisA2 = rbA.getCenterOfMassTransform().getBasis().getColumn(1);
	} else if (projection <= -1.0f + SIMD_EPSILON) {
		rbAxisA1 = rbA.getCenterOfMassTransform().getBasis().getColumn(2);
		rbAxisA2 = rbA.getCenterOfMassTransform().getBasis().getColumn(1);      
	} else {
		rbAxisA2 = axisInA.cross(rbAxisA1);
		rbAxisA1 = rbAxisA2.cross(axisInA);
	}

	m_rbAFrame.getBasis().setValue( rbAxisA1.getX(),rbAxisA2.getX(),axisInA.getX(),
									rbAxisA1.getY(),rbAxisA2.getY(),axisInA.getY(),
									rbAxisA1.getZ(),rbAxisA2.getZ(),axisInA.getZ() );

	btQuaternion rotationArc = shortestArcQuat(axisInA,axisInB);
	btVector3 rbAxisB1 =  quatRotate(rotationArc,rbAxisA1);
	btVector3 rbAxisB2 =  axisInB.cross(rbAxisB1);	
	
	m_rbBFrame.getOrigin() = pivotInB;
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
	m_referenceSign = m_useReferenceFrameA ? btScalar(-1.f) : btScalar(1.f);
}

//-----------------------------------------------------------------------------

btHingeConstraint::btHingeConstraint(btRigidBody& rbA,const btVector3& pivotInA,btVector3& axisInA, bool useReferenceFrameA)
:btTypedConstraint(HINGE_CONSTRAINT_TYPE, rbA), m_angularOnly(false), m_enableAngularMotor(false), 
m_useSolveConstraintObsolete(HINGE_USE_OBSOLETE_SOLVER),
m_useReferenceFrameA(useReferenceFrameA)
{

	// since no frame is given, assume this to be zero angle and just pick rb transform axis
	// fixed axis in worldspace
	btVector3 rbAxisA1, rbAxisA2;
	btPlaneSpace1(axisInA, rbAxisA1, rbAxisA2);

	m_rbAFrame.getOrigin() = pivotInA;
	m_rbAFrame.getBasis().setValue( rbAxisA1.getX(),rbAxisA2.getX(),axisInA.getX(),
									rbAxisA1.getY(),rbAxisA2.getY(),axisInA.getY(),
									rbAxisA1.getZ(),rbAxisA2.getZ(),axisInA.getZ() );

	btVector3 axisInB = rbA.getCenterOfMassTransform().getBasis() * axisInA;

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
	m_referenceSign = m_useReferenceFrameA ? btScalar(-1.f) : btScalar(1.f);
}

//-----------------------------------------------------------------------------

btHingeConstraint::btHingeConstraint(btRigidBody& rbA,btRigidBody& rbB, 
								     const btTransform& rbAFrame, const btTransform& rbBFrame, bool useReferenceFrameA)
:btTypedConstraint(HINGE_CONSTRAINT_TYPE, rbA,rbB),m_rbAFrame(rbAFrame),m_rbBFrame(rbBFrame),
m_angularOnly(false),
m_enableAngularMotor(false),
m_useSolveConstraintObsolete(HINGE_USE_OBSOLETE_SOLVER),
m_useReferenceFrameA(useReferenceFrameA)
{
	//start with free
	m_lowerLimit = btScalar(1e30);
	m_upperLimit = btScalar(-1e30);
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;
	m_limitSoftness = 0.9f;
	m_solveLimit = false;
	m_referenceSign = m_useReferenceFrameA ? btScalar(-1.f) : btScalar(1.f);
}			

//-----------------------------------------------------------------------------

btHingeConstraint::btHingeConstraint(btRigidBody& rbA, const btTransform& rbAFrame, bool useReferenceFrameA)
:btTypedConstraint(HINGE_CONSTRAINT_TYPE, rbA),m_rbAFrame(rbAFrame),m_rbBFrame(rbAFrame),
m_angularOnly(false),
m_enableAngularMotor(false),
m_useSolveConstraintObsolete(HINGE_USE_OBSOLETE_SOLVER),
m_useReferenceFrameA(useReferenceFrameA)
{
	///not providing rigidbody B means implicitly using worldspace for body B

	m_rbBFrame.getOrigin() = m_rbA.getCenterOfMassTransform()(m_rbAFrame.getOrigin());

	//start with free
	m_lowerLimit = btScalar(1e30);
	m_upperLimit = btScalar(-1e30);	
	m_biasFactor = 0.3f;
	m_relaxationFactor = 1.0f;
	m_limitSoftness = 0.9f;
	m_solveLimit = false;
	m_referenceSign = m_useReferenceFrameA ? btScalar(-1.f) : btScalar(1.f);
}

//-----------------------------------------------------------------------------

void	btHingeConstraint::buildJacobian()
{
	if (m_useSolveConstraintObsolete)
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

			// clear accumulator
			m_accLimitImpulse = btScalar(0.);

			// test angular limit
			testLimit();

		//Compute K = J*W*J' for hinge axis
		btVector3 axisA =  getRigidBodyA().getCenterOfMassTransform().getBasis() *  m_rbAFrame.getBasis().getColumn(2);
		m_kHinge =   1.0f / (getRigidBodyA().computeAngularImpulseDenominator(axisA) +
							 getRigidBodyB().computeAngularImpulseDenominator(axisA));

	}
}

//-----------------------------------------------------------------------------

void btHingeConstraint::getInfo1(btConstraintInfo1* info)
{
	if (m_useSolveConstraintObsolete)
	{
		info->m_numConstraintRows = 0;
		info->nub = 0;
	}
	else
	{
		info->m_numConstraintRows = 5; // Fixed 3 linear + 2 angular
		info->nub = 1; 
		//prepare constraint
		testLimit();
		if(getSolveLimit() || getEnableAngularMotor())
		{
			info->m_numConstraintRows++; // limit 3rd anguar as well
			info->nub--; 
		}
	}
} // btHingeConstraint::getInfo1 ()

//-----------------------------------------------------------------------------

void btHingeConstraint::getInfo2 (btConstraintInfo2* info)
{
	btAssert(!m_useSolveConstraintObsolete);
	int i, s = info->rowskip;
	// transforms in world space
	btTransform trA = m_rbA.getCenterOfMassTransform()*m_rbAFrame;
	btTransform trB = m_rbB.getCenterOfMassTransform()*m_rbBFrame;
	// pivot point
	btVector3 pivotAInW = trA.getOrigin();
	btVector3 pivotBInW = trB.getOrigin();
	// linear (all fixed)
    info->m_J1linearAxis[0] = 1;
    info->m_J1linearAxis[s + 1] = 1;
    info->m_J1linearAxis[2 * s + 2] = 1;
	btVector3 a1 = pivotAInW - m_rbA.getCenterOfMassTransform().getOrigin();
	{
		btVector3* angular0 = (btVector3*)(info->m_J1angularAxis);
		btVector3* angular1 = (btVector3*)(info->m_J1angularAxis + s);
		btVector3* angular2 = (btVector3*)(info->m_J1angularAxis + 2 * s);
		btVector3 a1neg = -a1;
		a1neg.getSkewSymmetricMatrix(angular0,angular1,angular2);
	}
	btVector3 a2 = pivotBInW - m_rbB.getCenterOfMassTransform().getOrigin();
	{
		btVector3* angular0 = (btVector3*)(info->m_J2angularAxis);
		btVector3* angular1 = (btVector3*)(info->m_J2angularAxis + s);
		btVector3* angular2 = (btVector3*)(info->m_J2angularAxis + 2 * s);
		a2.getSkewSymmetricMatrix(angular0,angular1,angular2);
	}
	// linear RHS
    btScalar k = info->fps * info->erp;
	for(i = 0; i < 3; i++)
    {
        info->m_constraintError[i * s] = k * (pivotBInW[i] - pivotAInW[i]);
    }
	// make rotations around X and Y equal
	// the hinge axis should be the only unconstrained
	// rotational axis, the angular velocity of the two bodies perpendicular to
	// the hinge axis should be equal. thus the constraint equations are
	//    p*w1 - p*w2 = 0
	//    q*w1 - q*w2 = 0
	// where p and q are unit vectors normal to the hinge axis, and w1 and w2
	// are the angular velocity vectors of the two bodies.
	// get hinge axis (Z)
	btVector3 ax1 = trA.getBasis().getColumn(2);
	// get 2 orthos to hinge axis (X, Y)
	btVector3 p = trA.getBasis().getColumn(0);
	btVector3 q = trA.getBasis().getColumn(1);
	// set the two hinge angular rows 
    int s3 = 3 * info->rowskip;
    int s4 = 4 * info->rowskip;

	info->m_J1angularAxis[s3 + 0] = p[0];
	info->m_J1angularAxis[s3 + 1] = p[1];
	info->m_J1angularAxis[s3 + 2] = p[2];
	info->m_J1angularAxis[s4 + 0] = q[0];
	info->m_J1angularAxis[s4 + 1] = q[1];
	info->m_J1angularAxis[s4 + 2] = q[2];

	info->m_J2angularAxis[s3 + 0] = -p[0];
	info->m_J2angularAxis[s3 + 1] = -p[1];
	info->m_J2angularAxis[s3 + 2] = -p[2];
	info->m_J2angularAxis[s4 + 0] = -q[0];
	info->m_J2angularAxis[s4 + 1] = -q[1];
	info->m_J2angularAxis[s4 + 2] = -q[2];
    // compute the right hand side of the constraint equation. set relative
    // body velocities along p and q to bring the hinge back into alignment.
    // if ax1,ax2 are the unit length hinge axes as computed from body1 and
    // body2, we need to rotate both bodies along the axis u = (ax1 x ax2).
    // if `theta' is the angle between ax1 and ax2, we need an angular velocity
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
    btVector3 ax2 = trB.getBasis().getColumn(2);
	btVector3 u = ax1.cross(ax2);
	info->m_constraintError[s3] = k * u.dot(p);
	info->m_constraintError[s4] = k * u.dot(q);
	// check angular limits
	int nrow = 4; // last filled row
	int srow;
	btScalar limit_err = btScalar(0.0);
	int limit = 0;
	if(getSolveLimit())
	{
		limit_err = m_correction * m_referenceSign;
		limit = (limit_err > btScalar(0.0)) ? 1 : 2;
	}
	// if the hinge has joint limits or motor, add in the extra row
	int powered = 0;
	if(getEnableAngularMotor())
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

		btScalar lostop = getLowerLimit();
		btScalar histop = getUpperLimit();
		if(limit && (lostop == histop))
		{  // the joint motor is ineffective
			powered = 0;
		}
		info->m_constraintError[srow] = btScalar(0.0f);
		if(powered)
		{
            info->cfm[srow] = btScalar(0.0); 
			btScalar mot_fact = getMotorFactor(m_hingeAngle, lostop, histop, m_motorTargetVelocity, info->fps * info->erp);
			info->m_constraintError[srow] += mot_fact * m_motorTargetVelocity * m_referenceSign;
			info->m_lowerLimit[srow] = - m_maxMotorImpulse;
			info->m_upperLimit[srow] =   m_maxMotorImpulse;
		}
		if(limit)
		{
			k = info->fps * info->erp;
			info->m_constraintError[srow] += k * limit_err;
			info->cfm[srow] = btScalar(0.0);
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
			btScalar bounce = m_relaxationFactor;
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
			info->m_constraintError[srow] *= m_biasFactor;
		} // if(limit)
	} // if angular limit or powered
}

//-----------------------------------------------------------------------------

void	btHingeConstraint::solveConstraintObsolete(btSolverBody& bodyA,btSolverBody& bodyB,btScalar	timeStep)
{

	///for backwards compatibility during the transition to 'getInfo/getInfo2'
	if (m_useSolveConstraintObsolete)
	{

		btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_rbAFrame.getOrigin();
		btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_rbBFrame.getOrigin();

		btScalar tau = btScalar(0.3);

		//linear part
		if (!m_angularOnly)
		{
			btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
			btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();

			btVector3 vel1,vel2;
			bodyA.getVelocityInLocalPointObsolete(rel_pos1,vel1);
			bodyB.getVelocityInLocalPointObsolete(rel_pos2,vel2);
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
				btVector3 ftorqueAxis1 = rel_pos1.cross(normal);
				btVector3 ftorqueAxis2 = rel_pos2.cross(normal);
				bodyA.applyImpulse(normal*m_rbA.getInvMass(), m_rbA.getInvInertiaTensorWorld()*ftorqueAxis1,impulse);
				bodyB.applyImpulse(normal*m_rbB.getInvMass(), m_rbB.getInvInertiaTensorWorld()*ftorqueAxis2,-impulse);
			}
		}

		
		{
			///solve angular part

			// get axes in world space
			btVector3 axisA =  getRigidBodyA().getCenterOfMassTransform().getBasis() *  m_rbAFrame.getBasis().getColumn(2);
			btVector3 axisB =  getRigidBodyB().getCenterOfMassTransform().getBasis() *  m_rbBFrame.getBasis().getColumn(2);

			btVector3 angVelA;
			bodyA.getAngularVelocity(angVelA);
			btVector3 angVelB;
			bodyB.getAngularVelocity(angVelB);

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
					//velrelOrthog *= (btScalar(1.)/denom) * m_relaxationFactor;

					bodyA.applyImpulse(btVector3(0,0,0), m_rbA.getInvInertiaTensorWorld()*velrelOrthog,-(btScalar(1.)/denom));
					bodyB.applyImpulse(btVector3(0,0,0), m_rbB.getInvInertiaTensorWorld()*velrelOrthog,(btScalar(1.)/denom));

				}

				//solve angular positional correction
				btVector3 angularError =  axisA.cross(axisB) *(btScalar(1.)/timeStep);
				btScalar len2 = angularError.length();
				if (len2>btScalar(0.00001))
				{
					btVector3 normal2 = angularError.normalized();
					btScalar denom2 = getRigidBodyA().computeAngularImpulseDenominator(normal2) +
							getRigidBodyB().computeAngularImpulseDenominator(normal2);
					//angularError *= (btScalar(1.)/denom2) * relaxation;
					
					bodyA.applyImpulse(btVector3(0,0,0), m_rbA.getInvInertiaTensorWorld()*angularError,(btScalar(1.)/denom2));
					bodyB.applyImpulse(btVector3(0,0,0), m_rbB.getInvInertiaTensorWorld()*angularError,-(btScalar(1.)/denom2));

				}
				
				



				// solve limit
				if (m_solveLimit)
				{
					btScalar amplitude = ( (angVelB - angVelA).dot( axisA )*m_relaxationFactor + m_correction* (btScalar(1.)/timeStep)*m_biasFactor  ) * m_limitSign;

					btScalar impulseMag = amplitude * m_kHinge;

					// Clamp the accumulated impulse
					btScalar temp = m_accLimitImpulse;
					m_accLimitImpulse = btMax(m_accLimitImpulse + impulseMag, btScalar(0) );
					impulseMag = m_accLimitImpulse - temp;


					
					bodyA.applyImpulse(btVector3(0,0,0), m_rbA.getInvInertiaTensorWorld()*axisA,impulseMag * m_limitSign);
					bodyB.applyImpulse(btVector3(0,0,0), m_rbB.getInvInertiaTensorWorld()*axisA,-(impulseMag * m_limitSign));

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
			
				bodyA.applyImpulse(btVector3(0,0,0), m_rbA.getInvInertiaTensorWorld()*axisA,clippedMotorImpulse);
				bodyB.applyImpulse(btVector3(0,0,0), m_rbB.getInvInertiaTensorWorld()*axisA,-clippedMotorImpulse);
				
			}
		}
	}

}

//-----------------------------------------------------------------------------

void	btHingeConstraint::updateRHS(btScalar	timeStep)
{
	(void)timeStep;

}

//-----------------------------------------------------------------------------

btScalar btHingeConstraint::getHingeAngle()
{
	const btVector3 refAxis0  = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_rbAFrame.getBasis().getColumn(0);
	const btVector3 refAxis1  = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_rbAFrame.getBasis().getColumn(1);
	const btVector3 swingAxis = getRigidBodyB().getCenterOfMassTransform().getBasis() * m_rbBFrame.getBasis().getColumn(1);
	btScalar angle = btAtan2Fast(swingAxis.dot(refAxis0), swingAxis.dot(refAxis1));
	return m_referenceSign * angle;
}

//-----------------------------------------------------------------------------

void btHingeConstraint::testLimit()
{
	// Compute limit information
	m_hingeAngle = getHingeAngle();  
	m_correction = btScalar(0.);
	m_limitSign = btScalar(0.);
	m_solveLimit = false;
	if (m_lowerLimit <= m_upperLimit)
	{
		if (m_hingeAngle <= m_lowerLimit)
		{
			m_correction = (m_lowerLimit - m_hingeAngle);
			m_limitSign = 1.0f;
			m_solveLimit = true;
		} 
		else if (m_hingeAngle >= m_upperLimit)
		{
			m_correction = m_upperLimit - m_hingeAngle;
			m_limitSign = -1.0f;
			m_solveLimit = true;
		}
	}
	return;
} // btHingeConstraint::testLimit()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
