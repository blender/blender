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
#ifndef POINT2POINTCONSTRAINT_H
#define POINT2POINTCONSTRAINT_H

#include "SimdVector3.h"

#include "ConstraintSolver/JacobianEntry.h"
class RigidBody;


/// point to point constraint between two rigidbodies each with a pivotpoint that descibes the 'ballsocket' location in local space
class Point2PointConstraint
{
	JacobianEntry	m_jac[3]; //3 orthogonal linear constraints
	RigidBody&	m_rbA;
	RigidBody&	m_rbB;

	SimdVector3	m_pivotInA;
	SimdVector3	m_pivotInB;
	
public:

	Point2PointConstraint(RigidBody& rbA,RigidBody& rbB, const SimdVector3& pivotInA,const SimdVector3& pivotInB);

	Point2PointConstraint(RigidBody& rbA,const SimdVector3& pivotInA);

	Point2PointConstraint();

	void	BuildJacobian();

	void	SolveConstraint(SimdScalar	timeStep);

	void	UpdateRHS(SimdScalar	timeStep);

	const RigidBody& GetRigidBodyA() const
	{
		return m_rbA;
	}
	const RigidBody& GetRigidBodyB() const
	{
		return m_rbB;
	}


};

#endif //POINT2POINTCONSTRAINT_H
