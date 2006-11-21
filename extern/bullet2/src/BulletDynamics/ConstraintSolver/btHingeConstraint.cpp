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


btHingeConstraint::btHingeConstraint()
{
}

btHingeConstraint::btHingeConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB,
								 btVector3& axisInA,btVector3& axisInB)
:btTypedConstraint(rbA,rbB),m_pivotInA(pivotInA),m_pivotInB(pivotInB),
m_axisInA(axisInA),
m_axisInB(-axisInB),
m_angularOnly(false)
{

}


btHingeConstraint::btHingeConstraint(btRigidBody& rbA,const btVector3& pivotInA,btVector3& axisInA)
:btTypedConstraint(rbA),m_pivotInA(pivotInA),m_pivotInB(rbA.getCenterOfMassTransform()(pivotInA)),
m_axisInA(axisInA),
//fixed axis in worldspace
m_axisInB(rbA.getCenterOfMassTransform().getBasis() * -axisInA),
m_angularOnly(false)
{
	
}

void	btHingeConstraint::buildJacobian()
{
	m_appliedImpulse = 0.f;

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
	btVector3 axisWorldA = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_axisInA;
	btVector3 jointAxis0;
	btVector3 jointAxis1;
	btPlaneSpace1(axisWorldA,jointAxis0,jointAxis1);
	
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


}

void	btHingeConstraint::solveConstraint(btScalar	timeStep)
{
//#define NEW_IMPLEMENTATION

#ifdef NEW_IMPLEMENTATION
	btScalar tau = 0.3f;
	btScalar damping = 1.f;

	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_pivotInA;
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_pivotInB;

	// Dirk: Don't we need to update this after each applied impulse
	btVector3 angvelA; // = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
    btVector3 angvelB; // = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();


	if (!m_angularOnly)
	{
		btVector3 normal(0,0,0);

		for (int i=0;i<3;i++)
		{		
			normal[i] = 1;
			btScalar jacDiagABInv = 1.f / m_jac[i].getDiagonal();

			btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
			btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
			
			btVector3 vel1 = m_rbA.getVelocityInLocalPoint(rel_pos1);
			btVector3 vel2 = m_rbB.getVelocityInLocalPoint(rel_pos2);
			btVector3 vel = vel1 - vel2;

			// Dirk: Get new angular velocity since it changed after applying an impulse
			angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
			angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();
	
			//velocity error (first order error)
			btScalar rel_vel = m_jac[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA, 
																	m_rbB.getLinearVelocity(),angvelB);
		
			//positional error (zeroth order error)
			btScalar depth = -(pivotAInW - pivotBInW).dot(normal); 
			
			btScalar impulse = tau*depth/timeStep * jacDiagABInv -  damping * rel_vel * jacDiagABInv;

			btVector3 impulse_vector = normal * impulse;
			m_rbA.applyImpulse( impulse_vector, pivotAInW - m_rbA.getCenterOfMassPosition());
			m_rbB.applyImpulse(-impulse_vector, pivotBInW - m_rbB.getCenterOfMassPosition());
			
			normal[i] = 0;
		}
	}

	///solve angular part

	// get axes in world space
	btVector3 axisA = getRigidBodyA().getCenterOfMassTransform().getBasis() * m_axisInA;
	btVector3 axisB = getRigidBodyB().getCenterOfMassTransform().getBasis() * m_axisInB;

	// constraint axes in world space
	btVector3 jointAxis0;
	btVector3 jointAxis1;
	btPlaneSpace1(axisA,jointAxis0,jointAxis1);


	// Dirk: Get new angular velocity since it changed after applying an impulse
	angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
    angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();
	
	btScalar jacDiagABInv0 = 1.f / m_jacAng[0].getDiagonal();
	btScalar rel_vel0 = m_jacAng[0].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA, 
																	m_rbB.getLinearVelocity(),angvelB);
	float tau1 = tau;//0.f;

	btScalar impulse0 = (tau1 * axisB.dot(jointAxis1) / timeStep - damping * rel_vel0) * jacDiagABInv0;
	btVector3 angular_impulse0 = jointAxis0 * impulse0;

	m_rbA.applyTorqueImpulse( angular_impulse0);
	m_rbB.applyTorqueImpulse(-angular_impulse0);



	// Dirk: Get new angular velocity since it changed after applying an impulse	
	angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
    angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();

	btScalar jacDiagABInv1 = 1.f / m_jacAng[1].getDiagonal();
	btScalar rel_vel1 = m_jacAng[1].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA, 
																	m_rbB.getLinearVelocity(),angvelB);;

	btScalar impulse1 = -(tau1 * axisB.dot(jointAxis0) / timeStep + damping * rel_vel1) * jacDiagABInv1;
	btVector3 angular_impulse1 = jointAxis1 * impulse1;

	m_rbA.applyTorqueImpulse( angular_impulse1);
	m_rbB.applyTorqueImpulse(-angular_impulse1);

#else


	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_pivotInA;
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_pivotInB;

	btVector3 normal(0,0,0);
	btScalar tau = 0.3f;
	btScalar damping = 1.f;

//linear part
	if (!m_angularOnly)
	{
		for (int i=0;i<3;i++)
		{		
			normal[i] = 1;
			btScalar jacDiagABInv = 1.f / m_jac[i].getDiagonal();

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
		btVector3 angA = angVelA - axisA * axisA.dot(angVelA);
		btVector3 angB = angVelB - axisB * axisB.dot(angVelB);
		btVector3 velrel = angA-angB;

		//solve angular velocity correction
		float relaxation = 1.f;
		float len = velrel.length();
		if (len > 0.00001f)
		{
			btVector3 normal = velrel.normalized();
			float denom = getRigidBodyA().computeAngularImpulseDenominator(normal) +
				getRigidBodyB().computeAngularImpulseDenominator(normal);
			// scale for mass and relaxation
			velrel *= (1.f/denom) * 0.9;
		}

		//solve angular positional correction
		btVector3 angularError = -axisA.cross(axisB) *(1.f/timeStep);
		float len2 = angularError.length();
		if (len2>0.00001f)
		{
			btVector3 normal2 = angularError.normalized();
			float denom2 = getRigidBodyA().computeAngularImpulseDenominator(normal2) +
					getRigidBodyB().computeAngularImpulseDenominator(normal2);
			angularError *= (1.f/denom2) * relaxation;
		}

		m_rbA.applyTorqueImpulse(-velrel+angularError);
		m_rbB.applyTorqueImpulse(velrel-angularError);
	}
#endif

}

void	btHingeConstraint::updateRHS(btScalar	timeStep)
{

}

