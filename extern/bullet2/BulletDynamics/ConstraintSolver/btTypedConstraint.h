/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2010 Erwin Coumans  http://continuousphysics.com/Bullet/

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
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"

class btSerializer;

enum btTypedConstraintType
{
	POINT2POINT_CONSTRAINT_TYPE=MAX_CONTACT_MANIFOLD_TYPE+1,
	HINGE_CONSTRAINT_TYPE,
	CONETWIST_CONSTRAINT_TYPE,
	D6_CONSTRAINT_TYPE,
	SLIDER_CONSTRAINT_TYPE,
	CONTACT_CONSTRAINT_TYPE
};


enum btConstraintParams
{
	BT_CONSTRAINT_ERP=1,
	BT_CONSTRAINT_STOP_ERP,
	BT_CONSTRAINT_CFM,
	BT_CONSTRAINT_STOP_CFM
};

#if 1
	#define btAssertConstrParams(_par) btAssert(_par) 
#else
	#define btAssertConstrParams(_par)
#endif


///TypedConstraint is the baseclass for Bullet constraints and vehicles
class btTypedConstraint : public btTypedObject
{
	int	m_userConstraintType;
	int	m_userConstraintId;
	bool m_needsFeedback;

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

	///internal method used by the constraint solver, don't use them directly
	btScalar getMotorFactor(btScalar pos, btScalar lowLim, btScalar uppLim, btScalar vel, btScalar timeFact);
	
	static btRigidBody& getFixedBody()
	{
		static btRigidBody s_fixed(0, 0,0);
		s_fixed.setMassProps(btScalar(0.),btVector3(btScalar(0.),btScalar(0.),btScalar(0.)));
		return s_fixed;
	}	


public:

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
		// number of solver iterations
		int m_numIterations;
	};

	///internal method used by the constraint solver, don't use them directly
	virtual void	buildJacobian() {};

	///internal method used by the constraint solver, don't use them directly
	virtual	void	setupSolverConstraint(btConstraintArray& ca, int solverBodyA,int solverBodyB, btScalar timeStep)
	{
        (void)ca;
        (void)solverBodyA;
        (void)solverBodyB;
        (void)timeStep;
	}
	
	///internal method used by the constraint solver, don't use them directly
	virtual void getInfo1 (btConstraintInfo1* info)=0;

	///internal method used by the constraint solver, don't use them directly
	virtual void getInfo2 (btConstraintInfo2* info)=0;

	///internal method used by the constraint solver, don't use them directly
	void	internalSetAppliedImpulse(btScalar appliedImpulse)
	{
		m_appliedImpulse = appliedImpulse;
	}
	///internal method used by the constraint solver, don't use them directly
	btScalar	internalGetAppliedImpulse()
	{
		return m_appliedImpulse;
	}

	///internal method used by the constraint solver, don't use them directly
	virtual	void	solveConstraintObsolete(btRigidBody& bodyA,btRigidBody& bodyB,btScalar	timeStep) {};

	
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

	bool	needsFeedback() const
	{
		return m_needsFeedback;
	}

	///enableFeedback will allow to read the applied linear and angular impulse
	///use getAppliedImpulse, getAppliedLinearImpulse and getAppliedAngularImpulse to read feedback information
	void	enableFeedback(bool needsFeedback)
	{
		m_needsFeedback = needsFeedback;
	}

	///getAppliedImpulse is an estimated total applied impulse. 
	///This feedback could be used to determine breaking constraints or playing sounds.
	btScalar	getAppliedImpulse() const
	{
		btAssert(m_needsFeedback);
		return m_appliedImpulse;
	}

	btTypedConstraintType getConstraintType () const
	{
		return btTypedConstraintType(m_objectType);
	}
	
	void setDbgDrawSize(btScalar dbgDrawSize)
	{
		m_dbgDrawSize = dbgDrawSize;
	}
	btScalar getDbgDrawSize()
	{
		return m_dbgDrawSize;
	}

	///override the default global value of a parameter (such as ERP or CFM), optionally provide the axis (0..5). 
	///If no axis is provided, it uses the default axis for this constraint.
	virtual	void	setParam(int num, btScalar value, int axis = -1) = 0;

	///return the local value of parameter
	virtual	btScalar getParam(int num, int axis = -1) const = 0;
	
	virtual	int	calculateSerializeBufferSize() const;

	///fills the dataBuffer and returns the struct name (and 0 on failure)
	virtual	const char*	serialize(void* dataBuffer, btSerializer* serializer) const;

};

// returns angle in range [-SIMD_2_PI, SIMD_2_PI], closest to one of the limits 
// all arguments should be normalized angles (i.e. in range [-SIMD_PI, SIMD_PI])
SIMD_FORCE_INLINE btScalar btAdjustAngleToLimits(btScalar angleInRadians, btScalar angleLowerLimitInRadians, btScalar angleUpperLimitInRadians)
{
	if(angleLowerLimitInRadians >= angleUpperLimitInRadians)
	{
		return angleInRadians;
	}
	else if(angleInRadians < angleLowerLimitInRadians)
	{
		btScalar diffLo = btNormalizeAngle(angleLowerLimitInRadians - angleInRadians); // this is positive
		btScalar diffHi = btFabs(btNormalizeAngle(angleUpperLimitInRadians - angleInRadians));
		return (diffLo < diffHi) ? angleInRadians : (angleInRadians + SIMD_2_PI);
	}
	else if(angleInRadians > angleUpperLimitInRadians)
	{
		btScalar diffHi = btNormalizeAngle(angleInRadians - angleUpperLimitInRadians); // this is positive
		btScalar diffLo = btFabs(btNormalizeAngle(angleInRadians - angleLowerLimitInRadians));
		return (diffLo < diffHi) ? (angleInRadians - SIMD_2_PI) : angleInRadians;
	}
	else
	{
		return angleInRadians;
	}
}

///do not change those serialization structures, it requires an updated sBulletDNAstr/sBulletDNAstr64
struct	btTypedConstraintData
{
	btRigidBodyData		*m_rbA;
	btRigidBodyData		*m_rbB;
	char	*m_name;

	int	m_objectType;
	int	m_userConstraintType;
	int	m_userConstraintId;
	int	m_needsFeedback;

	float	m_appliedImpulse;
	float	m_dbgDrawSize;

	int	m_disableCollisionsBetweenLinkedBodies;
	char	m_pad4[4];
	
};

SIMD_FORCE_INLINE	int	btTypedConstraint::calculateSerializeBufferSize() const
{
	return sizeof(btTypedConstraintData);
}




#endif //TYPED_CONSTRAINT_H
