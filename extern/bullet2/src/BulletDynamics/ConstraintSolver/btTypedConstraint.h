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

#ifndef TYPED_CONSTRAINT_H
#define TYPED_CONSTRAINT_H

class btRigidBody;
#include "../../LinearMath/btScalar.h"

///TypedConstraint is the baseclass for Bullet constraints and vehicles
class btTypedConstraint
{
	int	m_userConstraintType;
	int	m_userConstraintId;

	btTypedConstraint&	operator=(btTypedConstraint&	other)
	{
		btAssert(0);
		(void) other;
		return *this;
	}

protected:
	btRigidBody&	m_rbA;
	btRigidBody&	m_rbB;
	btScalar	m_appliedImpulse;


public:

	btTypedConstraint();
	virtual ~btTypedConstraint() {};
	btTypedConstraint(btRigidBody& rbA);

	btTypedConstraint(btRigidBody& rbA,btRigidBody& rbB);

	virtual void	buildJacobian() = 0;

	virtual	void	solveConstraint(btScalar	timeStep) = 0;

	const btRigidBody& getRigidBodyA() const
	{
		return m_rbA;
	}
	const btRigidBody& getRigidBodyB() const
	{
		return m_rbB;
	}

	btRigidBody& getRigidBodyA()
	{
		return m_rbA;
	}
	btRigidBody& getRigidBodyB()
	{
		return m_rbB;
	}

	int getUserConstraintType() const
	{
		return m_userConstraintType ;
	}

	void	setUserConstraintType(int userConstraintType)
	{
		m_userConstraintType = userConstraintType;
	};

	void	setUserConstraintId(int uid)
	{
		m_userConstraintId = uid;
	}
	
	int getUserConstraintId()
	{
		return m_userConstraintId;
	}
	btScalar	getAppliedImpulse()
	{
		return m_appliedImpulse;
	}
};

#endif //TYPED_CONSTRAINT_H
