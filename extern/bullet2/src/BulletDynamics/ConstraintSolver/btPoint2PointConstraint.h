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

#include "LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btTypedConstraint.h"

class btRigidBody;

struct	btConstraintSetting
{
	btConstraintSetting()	:
		m_tau(btScalar(0.3)),
		m_damping(btScalar(1.)),
		m_impulseClamp(btScalar(0.))
	{
	}
	btScalar		m_tau;
	btScalar		m_damping;
	btScalar		m_impulseClamp;
};

/// point to point constraint between two rigidbodies each with a pivotpoint that descibes the 'ballsocket' location in local space
class btPoint2PointConstraint : public btTypedConstraint
{
#ifdef IN_PARALLELL_SOLVER
public:
#endif
	btJacobianEntry	m_jac[3]; //3 orthogonal linear constraints
	
	btVector3	m_pivotInA;
	btVector3	m_pivotInB;
	
	
	
public:

	///for backwards compatibility during the transition to 'getInfo/getInfo2'
	bool		m_useSolveConstraintObsolete;

	btConstraintSetting	m_setting;

	btPoint2PointConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB);

	btPoint2PointConstraint(btRigidBody& rbA,const btVector3& pivotInA);

	btPoint2PointConstraint();

	virtual void	buildJacobian();

	virtual void getInfo1 (btConstraintInfo1* info);

	virtual void getInfo2 (btConstraintInfo2* info);


	virtual	void	solveConstraintObsolete(btSolverBody& bodyA,btSolverBody& bodyB,btScalar	timeStep);

	void	updateRHS(btScalar	timeStep);

	void	setPivotA(const btVector3& pivotA)
	{
		m_pivotInA = pivotA;
	}

	void	setPivotB(const btVector3& pivotB)
	{
		m_pivotInB = pivotB;
	}

	const btVector3& getPivotInA() const
	{
		return m_pivotInA;
	}

	const btVector3& getPivotInB() const
	{
		return m_pivotInB;
	}


};

#endif //POINT2POINTCONSTRAINT_H
