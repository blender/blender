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


#include "Point2PointConstraint.h"
#include "Dynamics/RigidBody.h"
#include "Dynamics/MassProps.h"




Point2PointConstraint::Point2PointConstraint()
{
}

Point2PointConstraint::Point2PointConstraint(RigidBody& rbA,RigidBody& rbB, const SimdVector3& pivotInA,const SimdVector3& pivotInB)
:TypedConstraint(rbA,rbB),m_pivotInA(pivotInA),m_pivotInB(pivotInB)
{

}


Point2PointConstraint::Point2PointConstraint(RigidBody& rbA,const SimdVector3& pivotInA)
:TypedConstraint(rbA),m_pivotInA(pivotInA),m_pivotInB(rbA.getCenterOfMassTransform()(pivotInA))
{
	
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
		
		SimdScalar impulse = depth*m_setting.m_tau/timeStep  * jacDiagABInv -  m_setting.m_damping * rel_vel * jacDiagABInv;

		SimdVector3 impulse_vector = normal * impulse;
		m_rbA.applyImpulse(impulse_vector, pivotAInW - m_rbA.getCenterOfMassPosition());
		m_rbB.applyImpulse(-impulse_vector, pivotBInW - m_rbB.getCenterOfMassPosition());
		
		normal[i] = 0;
	}
}

void	Point2PointConstraint::UpdateRHS(SimdScalar	timeStep)
{

}

