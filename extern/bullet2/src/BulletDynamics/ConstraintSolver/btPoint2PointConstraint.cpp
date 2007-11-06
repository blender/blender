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


#include "btPoint2PointConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include <new>



btPoint2PointConstraint::btPoint2PointConstraint()
:btTypedConstraint(POINT2POINT_CONSTRAINT_TYPE)
{
}

btPoint2PointConstraint::btPoint2PointConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB)
:btTypedConstraint(POINT2POINT_CONSTRAINT_TYPE,rbA,rbB),m_pivotInA(pivotInA),m_pivotInB(pivotInB)
{

}


btPoint2PointConstraint::btPoint2PointConstraint(btRigidBody& rbA,const btVector3& pivotInA)
:btTypedConstraint(POINT2POINT_CONSTRAINT_TYPE,rbA),m_pivotInA(pivotInA),m_pivotInB(rbA.getCenterOfMassTransform()(pivotInA))
{
	
}

void	btPoint2PointConstraint::buildJacobian()
{
	m_appliedImpulse = btScalar(0.);

	btVector3	normal(0,0,0);

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

void	btPoint2PointConstraint::solveConstraint(btScalar	timeStep)
{
	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_pivotInA;
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_pivotInB;


	btVector3 normal(0,0,0);
	

//	btVector3 angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
//	btVector3 angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();

	for (int i=0;i<3;i++)
	{		
		normal[i] = 1;
		btScalar jacDiagABInv = btScalar(1.) / m_jac[i].getDiagonal();

		btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
		btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
		//this jacobian entry could be re-used for all iterations
		
		btVector3 vel1 = m_rbA.getVelocityInLocalPoint(rel_pos1);
		btVector3 vel2 = m_rbB.getVelocityInLocalPoint(rel_pos2);
		btVector3 vel = vel1 - vel2;
		
		btScalar rel_vel;
		rel_vel = normal.dot(vel);

	/*
		//velocity error (first order error)
		btScalar rel_vel = m_jac[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA,
														m_rbB.getLinearVelocity(),angvelB);
	*/
	
		//positional error (zeroth order error)
		btScalar depth = -(pivotAInW - pivotBInW).dot(normal); //this is the error projected on the normal
		
		btScalar impulse = depth*m_setting.m_tau/timeStep  * jacDiagABInv -  m_setting.m_damping * rel_vel * jacDiagABInv;
		m_appliedImpulse+=impulse;
		btVector3 impulse_vector = normal * impulse;
		m_rbA.applyImpulse(impulse_vector, pivotAInW - m_rbA.getCenterOfMassPosition());
		m_rbB.applyImpulse(-impulse_vector, pivotBInW - m_rbB.getCenterOfMassPosition());
		
		normal[i] = 0;
	}
}

void	btPoint2PointConstraint::updateRHS(btScalar	timeStep)
{
	(void)timeStep;

}

