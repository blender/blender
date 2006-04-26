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


#include "HingeConstraint.h"
#include "Dynamics/RigidBody.h"
#include "Dynamics/MassProps.h"
#include "SimdTransformUtil.h"


HingeConstraint::HingeConstraint()
{
}

HingeConstraint::HingeConstraint(RigidBody& rbA,RigidBody& rbB, const SimdVector3& pivotInA,const SimdVector3& pivotInB,
								 SimdVector3& axisInA,SimdVector3& axisInB)
:TypedConstraint(rbA,rbB),m_pivotInA(pivotInA),m_pivotInB(pivotInB),
m_axisInA(axisInA),
m_axisInB(axisInB),
m_angularOnly(false)
{

}


HingeConstraint::HingeConstraint(RigidBody& rbA,const SimdVector3& pivotInA,SimdVector3& axisInA)
:TypedConstraint(rbA),m_pivotInA(pivotInA),m_pivotInB(rbA.getCenterOfMassTransform()(pivotInA)),
m_axisInA(axisInA),
//fixed axis in worldspace
m_axisInB(rbA.getCenterOfMassTransform().getBasis() * -axisInA),
m_angularOnly(false)
{
	
}

void	HingeConstraint::BuildJacobian()
{
	SimdVector3	normal(0,0,0);

	if (!m_angularOnly)
	{
		for (int i=0;i<3;i++)
		{
			normal[i] = 1;
			new (&m_jac[i]) JacobianEntry(
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

	//this is ununsed for now, it's a todo
	SimdVector3 axisWorldA = GetRigidBodyA().getCenterOfMassTransform().getBasis() * m_axisInA;
	SimdVector3 jointAxis0;
	SimdVector3 jointAxis1;
	SimdPlaneSpace1(axisWorldA,jointAxis0,jointAxis1);
	
	new (&m_jacAng[0])	JacobianEntry(jointAxis0,
		m_rbA.getCenterOfMassTransform().getBasis().transpose(),
		m_rbB.getCenterOfMassTransform().getBasis().transpose(),
		m_rbA.getInvInertiaDiagLocal(),
		m_rbB.getInvInertiaDiagLocal());

	new (&m_jacAng[1])	JacobianEntry(jointAxis1,
		m_rbA.getCenterOfMassTransform().getBasis().transpose(),
		m_rbB.getCenterOfMassTransform().getBasis().transpose(),
		m_rbA.getInvInertiaDiagLocal(),
		m_rbB.getInvInertiaDiagLocal());


}

void	HingeConstraint::SolveConstraint(SimdScalar	timeStep)
{

	SimdVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_pivotInA;
	SimdVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_pivotInB;

	SimdVector3 normal(0,0,0);
	SimdScalar tau = 0.3f;
	SimdScalar damping = 1.f;

	if (!m_angularOnly)
	{
		for (int i=0;i<3;i++)
		{		
			normal[i] = 1;
			SimdScalar jacDiagABInv = 1.f / m_jac[i].getDiagonal();

			SimdVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
			SimdVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
			
			SimdVector3 vel1 = m_rbA.getVelocityInLocalPoint(rel_pos1);
			SimdVector3 vel2 = m_rbB.getVelocityInLocalPoint(rel_pos2);
			SimdVector3 vel = vel1 - vel2;
			SimdScalar rel_vel;
			rel_vel = normal.dot(vel);
			//positional error (zeroth order error)
			SimdScalar depth = -(pivotAInW - pivotBInW).dot(normal); //this is the error projected on the normal
			SimdScalar impulse = depth*tau/timeStep  * jacDiagABInv -  damping * rel_vel * jacDiagABInv * damping;

			SimdVector3 impulse_vector = normal * impulse;
			m_rbA.applyImpulse(impulse_vector, pivotAInW - m_rbA.getCenterOfMassPosition());
			m_rbB.applyImpulse(-impulse_vector, pivotBInW - m_rbB.getCenterOfMassPosition());
			
			normal[i] = 0;
		}
	}

	///solve angular part

	// get axes in world space
	SimdVector3 axisA = GetRigidBodyA().getCenterOfMassTransform().getBasis() * m_axisInA;
	SimdVector3 axisB = GetRigidBodyB().getCenterOfMassTransform().getBasis() * m_axisInB;

	const SimdVector3& angVelA = GetRigidBodyA().getAngularVelocity();
	const SimdVector3& angVelB = GetRigidBodyB().getAngularVelocity();
	SimdVector3 angA = angVelA - axisA * axisA.dot(angVelA);
	SimdVector3 angB = angVelB - axisB * axisB.dot(angVelB);
	SimdVector3 velrel = angA-angB;

	//solve angular velocity correction
	float relaxation = 1.f;
	float len = velrel.length();
	if (len > 0.00001f)
	{
		SimdVector3 normal = velrel.normalized();
		float denom = GetRigidBodyA().ComputeAngularImpulseDenominator(normal) +
			GetRigidBodyB().ComputeAngularImpulseDenominator(normal);
		// scale for mass and relaxation
		velrel *= (1.f/denom) * 0.9;
	}

	//solve angular positional correction
	SimdVector3 angularError = -axisA.cross(axisB) *(1.f/timeStep);
	float len2 = angularError.length();
	if (len2>0.00001f)
	{
		SimdVector3 normal2 = angularError.normalized();
		float denom2 = GetRigidBodyA().ComputeAngularImpulseDenominator(normal2) +
				GetRigidBodyB().ComputeAngularImpulseDenominator(normal2);
		angularError *= (1.f/denom2) * relaxation;
	}

	m_rbA.applyTorqueImpulse(-velrel+angularError);
	m_rbB.applyTorqueImpulse(velrel-angularError);

}

void	HingeConstraint::UpdateRHS(SimdScalar	timeStep)
{

}

