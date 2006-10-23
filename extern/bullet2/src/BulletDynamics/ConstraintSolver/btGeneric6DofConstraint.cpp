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


#include "btGeneric6DofConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"

static const btScalar kSign[] = { 1.0f, -1.0f, 1.0f };
static const int kAxisA[] = { 1, 0, 0 };
static const int kAxisB[] = { 2, 2, 1 };

btGeneric6DofConstraint::btGeneric6DofConstraint()
{
}

btGeneric6DofConstraint::btGeneric6DofConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB)
: btTypedConstraint(rbA, rbB)
, m_frameInA(frameInA)
, m_frameInB(frameInB)
{
	//free means upper < lower, 
	//locked means upper == lower
	//limited means upper > lower
	//so start all locked
	for (int i=0; i<6;++i)
	{
		m_lowerLimit[i] = 0.0f;
		m_upperLimit[i] = 0.0f;
		m_accumulatedImpulse[i] = 0.0f;
	}

}


void btGeneric6DofConstraint::buildJacobian()
{
	btVector3	normal(0,0,0);

	const btVector3& pivotInA = m_frameInA.getOrigin();
	const btVector3& pivotInB = m_frameInB.getOrigin();

	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform() * m_frameInA.getOrigin();
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform() * m_frameInB.getOrigin();

	btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();

	int i;
	//linear part
	for (i=0;i<3;i++)
	{
		if (isLimited(i))
		{
			normal[i] = 1;
			
			// Create linear atom
			new (&m_jacLinear[i]) btJacobianEntry(
				m_rbA.getCenterOfMassTransform().getBasis().transpose(),
				m_rbB.getCenterOfMassTransform().getBasis().transpose(),
				m_rbA.getCenterOfMassTransform()*pivotInA - m_rbA.getCenterOfMassPosition(),
				m_rbB.getCenterOfMassTransform()*pivotInB - m_rbB.getCenterOfMassPosition(),
				normal,
				m_rbA.getInvInertiaDiagLocal(),
				m_rbA.getInvMass(),
				m_rbB.getInvInertiaDiagLocal(),
				m_rbB.getInvMass());

			// Apply accumulated impulse
			btVector3 impulse_vector = m_accumulatedImpulse[i] * normal;

			m_rbA.applyImpulse( impulse_vector, rel_pos1);
			m_rbB.applyImpulse(-impulse_vector, rel_pos2);

			normal[i] = 0;
		}
	}

	// angular part
	for (i=0;i<3;i++)
	{
		if (isLimited(i+3))
		{
			btVector3 axisA = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn( kAxisA[i] );
			btVector3 axisB = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn( kAxisB[i] );

			// Dirk: This is IMO mathematically the correct way, but we should consider axisA and axisB being near parallel maybe
			btVector3 axis = kSign[i] * axisA.cross(axisB);

			// Create angular atom
			new (&m_jacAng[i])	btJacobianEntry(axis,
				m_rbA.getCenterOfMassTransform().getBasis().transpose(),
				m_rbB.getCenterOfMassTransform().getBasis().transpose(),
				m_rbA.getInvInertiaDiagLocal(),
				m_rbB.getInvInertiaDiagLocal());

			// Apply accumulated impulse
			btVector3 impulse_vector = m_accumulatedImpulse[i + 3] * axis;

			m_rbA.applyTorqueImpulse( impulse_vector);
			m_rbB.applyTorqueImpulse(-impulse_vector);
		}
	}
}

void	btGeneric6DofConstraint::solveConstraint(btScalar	timeStep)
{
	btScalar tau = 0.1f;
	btScalar damping = 1.0f;

	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform() * m_frameInA.getOrigin();
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform() * m_frameInB.getOrigin();

	btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
	
	btVector3 normal(0,0,0);
	int i;

	// linear
	for (i=0;i<3;i++)
	{		
		if (isLimited(i))
		{
			btVector3 angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
			btVector3 angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();
		

			normal[i] = 1;
			btScalar jacDiagABInv = 1.f / m_jacLinear[i].getDiagonal();

			//velocity error (first order error)
			btScalar rel_vel = m_jacLinear[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA, 
																	m_rbB.getLinearVelocity(),angvelB);
		
			//positional error (zeroth order error)
			btScalar depth = -(pivotAInW - pivotBInW).dot(normal); 
			
			btScalar impulse = (tau*depth/timeStep - damping*rel_vel) * jacDiagABInv;
			m_accumulatedImpulse[i] += impulse;

			btVector3 impulse_vector = normal * impulse;
			m_rbA.applyImpulse( impulse_vector, rel_pos1);
			m_rbB.applyImpulse(-impulse_vector, rel_pos2);
			
			normal[i] = 0;
		}
	}

	// angular
	for (i=0;i<3;i++)
	{
		if (isLimited(i+3))
		{
			btVector3 angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
			btVector3 angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();
		
			btScalar jacDiagABInv = 1.f / m_jacAng[i].getDiagonal();
			
			//velocity error (first order error)
			btScalar rel_vel = m_jacAng[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA, 
																			m_rbB.getLinearVelocity(),angvelB);

			//positional error (zeroth order error)
			btVector3 axisA = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn( kAxisA[i] );
			btVector3 axisB = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn( kAxisB[i] );

			btScalar rel_pos = kSign[i] * axisA.dot(axisB);

			//impulse
			btScalar impulse = -(tau*rel_pos/timeStep + damping*rel_vel) * jacDiagABInv;
			m_accumulatedImpulse[i + 3] += impulse;
			
			// Dirk: Not needed - we could actually project onto Jacobian entry here (same as above)
			btVector3 axis = kSign[i] * axisA.cross(axisB);
			btVector3 impulse_vector = axis * impulse;

			m_rbA.applyTorqueImpulse( impulse_vector);
			m_rbB.applyTorqueImpulse(-impulse_vector);
		}
	}
}

void	btGeneric6DofConstraint::updateRHS(btScalar	timeStep)
{

}

btScalar btGeneric6DofConstraint::computeAngle(int axis) const
	{
	btScalar angle;

	switch (axis)
		{
		case 0:
			{
			btVector3 v1 = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn(1);
			btVector3 v2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(1);
			btVector3 w2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(2);

			btScalar s = v1.dot(w2);
			btScalar c = v1.dot(v2);

			angle = btAtan2( s, c );
			}
			break;

		case 1:
			{
			btVector3 w1 = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn(2);
			btVector3 w2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(2);
			btVector3 u2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(0);

			btScalar s = w1.dot(u2);
			btScalar c = w1.dot(w2);

			angle = btAtan2( s, c );
			}
			break;

		case 2:
			{
			btVector3 u1 = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn(0);
			btVector3 u2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(0);
			btVector3 v2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(1);

			btScalar s = u1.dot(v2);
			btScalar c = u1.dot(u2);

			angle = btAtan2( s, c );
			}
			break;
                  default: assert ( 0 ) ; break ;
		}

	return angle;
	}

