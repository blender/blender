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

#ifndef HINGECONSTRAINT_H
#define HINGECONSTRAINT_H

#include "../../LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btTypedConstraint.h"

class btRigidBody;


/// hinge constraint between two rigidbodies each with a pivotpoint that descibes the axis location in local space
/// axis defines the orientation of the hinge axis
class btHingeConstraint : public btTypedConstraint
{
	btJacobianEntry	m_jac[3]; //3 orthogonal linear constraints
	btJacobianEntry	m_jacAng[3]; //2 orthogonal angular constraints+ 1 for limit/motor

	btVector3	m_pivotInA;
	btVector3	m_pivotInB;
	btVector3	m_axisInA;
	btVector3	m_axisInB;

	bool		m_angularOnly;

	btScalar		m_motorTargetVelocity;
	btScalar		m_maxMotorImpulse;
	bool		m_enableAngularMotor;
	
public:

	btHingeConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB,btVector3& axisInA,btVector3& axisInB);

	btHingeConstraint(btRigidBody& rbA,const btVector3& pivotInA,btVector3& axisInA);

	btHingeConstraint();

	virtual void	buildJacobian();

	virtual	void	solveConstraint(btScalar	timeStep);

	void	updateRHS(btScalar	timeStep);

	const btRigidBody& getRigidBodyA() const
	{
		return m_rbA;
	}
	const btRigidBody& getRigidBodyB() const
	{
		return m_rbB;
	}

	void	setAngularOnly(bool angularOnly)
	{
		m_angularOnly = angularOnly;
	}

	void	enableAngularMotor(bool enableMotor,btScalar targetVelocity,btScalar maxMotorImpulse)
	{
		m_enableAngularMotor  = enableMotor;
		m_motorTargetVelocity = targetVelocity;
		m_maxMotorImpulse = maxMotorImpulse;
	}

};

#endif //HINGECONSTRAINT_H
