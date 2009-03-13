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
#include "LinearMath/btScalar.h"
#include "btSolverConstraint.h"
struct  btSolverBody;




enum btTypedConstraintType
{
	POINT2POINT_CONSTRAINT_TYPE,
	HINGE_CONSTRAINT_TYPE,
	CONETWIST_CONSTRAINT_TYPE,
	D6_CONSTRAINT_TYPE,
	SLIDER_CONSTRAINT_TYPE
};

///TypedConstraint is the baseclass for Bullet constraints and vehicles
class btTypedConstraint
{
	int	m_userConstraintType;
	int	m_userConstraintId;

	btTypedConstraintType m_constraintType;

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
	btScalar	m_dbgDrawSize;


public:

	btTypedConstraint(btTypedConstraintType type);
	virtual ~btTypedConstraint() {};
	btTypedConstraint(btTypedConstraintType type, btRigidBody& rbA);
	btTypedConstraint(btTypedConstraintType type, btRigidBody& rbA,btRigidBody& rbB);

	struct btConstraintInfo1 {
		int m_numConstraintRows,nub;
	};

	struct btConstraintInfo2 {
		// integrator parameters: frames per second (1/stepsize), default error
		// reduction parameter (0..1).
		btScalar fps,erp;

		// for the first and second body, pointers to two (linear and angular)
		// n*3 jacobian sub matrices, stored by rows. these matrices will have
		// been initialized to 0 on entry. if the second body is zero then the
		// J2xx pointers may be 0.
		btScalar *m_J1linearAxis,*m_J1angularAxis,*m_J2linearAxis,*m_J2angularAxis;

		// elements to jump from one row to the next in J's
		int rowskip;

		// right hand sides of the equation J*v = c + cfm * lambda. cfm is the
		// "constraint force mixing" vector. c is set to zero on entry, cfm is
		// set to a constant value (typically very small or zero) value on entry.
		btScalar *m_constraintError,*cfm;

		// lo and hi limits for variables (set to -/+ infinity on entry).
		btScalar *m_lowerLimit,*m_upperLimit;

		// findex vector for variables. see the LCP solver interface for a
		// description of what this does. this is set to -1 on entry.
		// note that the returned indexes are relative to the first index of
		// the constraint.
		int *findex;
	};


	virtual void	buildJacobian() = 0;

	virtual	void	setupSolverConstraint(btConstraintArray& ca, int solverBodyA,int solverBodyB, btScalar timeStep)
	{
	}
	virtual void getInfo1 (btConstraintInfo1* info)=0;

	virtual void getInfo2 (btConstraintInfo2* info)=0;

	virtual	void	solveConstraintObsolete(btSolverBody& bodyA,btSolverBody& bodyB,btScalar	timeStep) = 0;

	btScalar getMotorFactor(btScalar pos, btScalar lowLim, btScalar uppLim, btScalar vel, btScalar timeFact);
	
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

	int getUserConstraintId() const
	{
		return m_userConstraintId;
	}

	int getUid() const
	{
		return m_userConstraintId;   
	} 

	btScalar	getAppliedImpulse() const
	{
		return m_appliedImpulse;
	}

	btTypedConstraintType getConstraintType () const
	{
		return m_constraintType;
	}
	
	void setDbgDrawSize(btScalar dbgDrawSize)
	{
		m_dbgDrawSize = dbgDrawSize;
	}
	btScalar getDbgDrawSize()
	{
		return m_dbgDrawSize;
	}
	
};

#endif //TYPED_CONSTRAINT_H
