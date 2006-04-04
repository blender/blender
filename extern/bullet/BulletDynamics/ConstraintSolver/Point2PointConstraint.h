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

#ifndef POINT2POINTCONSTRAINT_H
#define POINT2POINTCONSTRAINT_H

#include "SimdVector3.h"

#include "ConstraintSolver/JacobianEntry.h"
#include "TypedConstraint.h"

class RigidBody;

struct	ConstraintSetting
{
	ConstraintSetting()	:
		m_tau(0.3f),
		m_damping(1.f)
	{
	}
	float		m_tau;
	float		m_damping;
};

/// point to point constraint between two rigidbodies each with a pivotpoint that descibes the 'ballsocket' location in local space
class Point2PointConstraint : public TypedConstraint
{
	JacobianEntry	m_jac[3]; //3 orthogonal linear constraints
	
	SimdVector3	m_pivotInA;
	SimdVector3	m_pivotInB;
	
	
	
public:

	ConstraintSetting	m_setting;

	Point2PointConstraint(RigidBody& rbA,RigidBody& rbB, const SimdVector3& pivotInA,const SimdVector3& pivotInB);

	Point2PointConstraint(RigidBody& rbA,const SimdVector3& pivotInA);

	Point2PointConstraint();

	virtual void	BuildJacobian();


	virtual	void	SolveConstraint(SimdScalar	timeStep);

	void	UpdateRHS(SimdScalar	timeStep);

	void	SetPivotA(const SimdVector3& pivotA)
	{
		m_pivotInA = pivotA;
	}

	void	SetPivotB(const SimdVector3& pivotB)
	{
		m_pivotInB = pivotB;
	}



};

#endif //POINT2POINTCONSTRAINT_H
