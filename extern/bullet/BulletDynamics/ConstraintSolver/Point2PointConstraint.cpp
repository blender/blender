/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#include "Point2PointConstraint.h"
#include "Dynamics/RigidBody.h"
#include "Dynamics/MassProps.h"


static RigidBody s_fixed(MassProps(0,SimdVector3(0.f,0.f,0.f)),0.f,0.f,1.f,1.f);

Point2PointConstraint::Point2PointConstraint():
m_rbA(s_fixed),m_rbB(s_fixed)
{
	s_fixed.setMassProps(0.f,SimdVector3(0.f,0.f,0.f));
}

Point2PointConstraint::Point2PointConstraint(RigidBody& rbA,RigidBody& rbB, const SimdVector3& pivotInA,const SimdVector3& pivotInB)
:m_rbA(rbA),m_rbB(rbB),m_pivotInA(pivotInA),m_pivotInB(pivotInB)
{

}


Point2PointConstraint::Point2PointConstraint(RigidBody& rbA,const SimdVector3& pivotInA)
:m_rbA(rbA),m_rbB(s_fixed),m_pivotInA(pivotInA),m_pivotInB(rbA.getCenterOfMassTransform()(pivotInA))
{
	s_fixed.setMassProps(0.f,SimdVector3(1e10f,1e10f,1e10f));
}

void	Point2PointConstraint::BuildJacobian()
{
	SimdVector3	normal(0,0,0);

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

void	Point2PointConstraint::SolveConstraint(SimdScalar	timeStep)
{
	SimdVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_pivotInA;
	SimdVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_pivotInB;


	SimdVector3 normal(0,0,0);
	SimdScalar tau = 0.3f;
	SimdScalar damping = 1.f;

//	SimdVector3 angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
//	SimdVector3 angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();

	for (int i=0;i<3;i++)
	{		
		normal[i] = 1;
		SimdScalar jacDiagABInv = 1.f / m_jac[i].getDiagonal();

		SimdVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
		SimdVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
		//this jacobian entry could be re-used for all iterations
		
		SimdVector3 vel1 = m_rbA.getVelocityInLocalPoint(rel_pos1);
		SimdVector3 vel2 = m_rbB.getVelocityInLocalPoint(rel_pos2);
		SimdVector3 vel = vel1 - vel2;
		
		SimdScalar rel_vel;
		rel_vel = normal.dot(vel);

	/*
		//velocity error (first order error)
		SimdScalar rel_vel = m_jac[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA,
														m_rbB.getLinearVelocity(),angvelB);
	*/
	
		//positional error (zeroth order error)
		SimdScalar depth = -(pivotAInW - pivotBInW).dot(normal); //this is the error projected on the normal
		
		SimdScalar impulse = depth*tau/timeStep  * jacDiagABInv -  damping * rel_vel * jacDiagABInv * damping;

		SimdVector3 impulse_vector = normal * impulse;
		m_rbA.applyImpulse(impulse_vector, pivotAInW - m_rbA.getCenterOfMassPosition());
		m_rbB.applyImpulse(-impulse_vector, pivotBInW - m_rbB.getCenterOfMassPosition());
		
		normal[i] = 0;
	}
}

void	Point2PointConstraint::UpdateRHS(SimdScalar	timeStep)
{

}

