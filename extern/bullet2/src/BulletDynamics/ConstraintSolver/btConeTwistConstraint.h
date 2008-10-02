/*
Bullet Continuous Collision Detection and Physics Library
btConeTwistConstraint is Copyright (c) 2007 Starbreeze Studios

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Written by: Marcus Hennix
*/



#ifndef CONETWISTCONSTRAINT_H
#define CONETWISTCONSTRAINT_H

#include "../../LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btTypedConstraint.h"

class btRigidBody;


///btConeTwistConstraint can be used to simulate ragdoll joints (upper arm, leg etc)
class btConeTwistConstraint : public btTypedConstraint
{
#ifdef IN_PARALLELL_SOLVER
public:
#endif
	btJacobianEntry	m_jac[3]; //3 orthogonal linear constraints

	btTransform m_rbAFrame; 
	btTransform m_rbBFrame;

	btScalar	m_limitSoftness;
	btScalar	m_biasFactor;
	btScalar	m_relaxationFactor;

	btScalar	m_swingSpan1;
	btScalar	m_swingSpan2;
	btScalar	m_twistSpan;

	btVector3   m_swingAxis;
	btVector3	m_twistAxis;

	btScalar	m_kSwing;
	btScalar	m_kTwist;

	btScalar	m_twistLimitSign;
	btScalar	m_swingCorrection;
	btScalar	m_twistCorrection;

	btScalar	m_accSwingLimitImpulse;
	btScalar	m_accTwistLimitImpulse;

	bool		m_angularOnly;
	bool		m_solveTwistLimit;
	bool		m_solveSwingLimit;

	
public:

	btConeTwistConstraint(btRigidBody& rbA,btRigidBody& rbB,const btTransform& rbAFrame, const btTransform& rbBFrame);
	
	btConeTwistConstraint(btRigidBody& rbA,const btTransform& rbAFrame);

	btConeTwistConstraint();

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

	void	setLimit(btScalar _swingSpan1,btScalar _swingSpan2,btScalar _twistSpan,  btScalar _softness = 0.8f, btScalar _biasFactor = 0.3f, btScalar _relaxationFactor = 1.0f)
	{
		m_swingSpan1 = _swingSpan1;
		m_swingSpan2 = _swingSpan2;
		m_twistSpan  = _twistSpan;

		m_limitSoftness =  _softness;
		m_biasFactor = _biasFactor;
		m_relaxationFactor = _relaxationFactor;
	}

	const btTransform& getAFrame() { return m_rbAFrame; };	
	const btTransform& getBFrame() { return m_rbBFrame; };

	inline int getSolveTwistLimit()
	{
		return m_solveTwistLimit;
	}

	inline int getSolveSwingLimit()
	{
		return m_solveTwistLimit;
	}

	inline btScalar getTwistLimitSign()
	{
		return m_twistLimitSign;
	}

};

#endif //CONETWISTCONSTRAINT_H
