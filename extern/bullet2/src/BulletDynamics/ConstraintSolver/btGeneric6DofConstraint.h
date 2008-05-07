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

#ifndef GENERIC_6DOF_CONSTRAINT_H
#define GENERIC_6DOF_CONSTRAINT_H

#include "../../LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btTypedConstraint.h"

class btRigidBody;



/// btGeneric6DofConstraint between two rigidbodies each with a pivotpoint that descibes the axis location in local space
/// btGeneric6DofConstraint can leave any of the 6 degree of freedom 'free' or 'locked'
/// Work in progress (is still a Hinge actually)
class btGeneric6DofConstraint : public btTypedConstraint
{
	btJacobianEntry	m_jacLinear[3];			// 3 orthogonal linear constraints
	btJacobianEntry	m_jacAng[3];		// 3 orthogonal angular constraints

	btTransform	m_frameInA;			// the constraint space w.r.t body A
	btTransform	m_frameInB;			// the constraint space w.r.t body B

	btScalar      m_lowerLimit[6];	// the constraint lower limits
	btScalar      m_upperLimit[6];	// the constraint upper limits

	btScalar		m_accumulatedImpulse[6];

	btGeneric6DofConstraint&	operator=(btGeneric6DofConstraint&	other)
	{
		btAssert(0);
		(void) other;
		return *this;
	}
		
public:
	btGeneric6DofConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB );

	btGeneric6DofConstraint();

	
	virtual void	buildJacobian();

	virtual	void	solveConstraint(btScalar	timeStep);

	void	updateRHS(btScalar	timeStep);

	btScalar computeAngle(int axis) const;

	void	setLinearLowerLimit(const btVector3& linearLower)
	{
		m_lowerLimit[0] = linearLower.getX();
		m_lowerLimit[1] = linearLower.getY();
		m_lowerLimit[2] = linearLower.getZ();
	}

	void	setLinearUpperLimit(const btVector3& linearUpper)
	{
		m_upperLimit[0] = linearUpper.getX();
		m_upperLimit[1] = linearUpper.getY();
		m_upperLimit[2] = linearUpper.getZ();
	}

	void	setAngularLowerLimit(const btVector3& angularLower)
	{
		m_lowerLimit[3] = angularLower.getX();
		m_lowerLimit[4] = angularLower.getY();
		m_lowerLimit[5] = angularLower.getZ();
	}

	void	setAngularUpperLimit(const btVector3& angularUpper)
	{
		m_upperLimit[3] = angularUpper.getX();
		m_upperLimit[4] = angularUpper.getY();
		m_upperLimit[5] = angularUpper.getZ();
	}

	//first 3 are linear, next 3 are angular
	void SetLimit(int axis, btScalar lo, btScalar hi)
	{
		m_lowerLimit[axis] = lo; 
		m_upperLimit[axis] = hi; 
	}

	//free means upper < lower, 
	//locked means upper == lower
	//limited means upper > lower
	//limitIndex: first 3 are linear, next 3 are angular
	bool	isLimited(int limitIndex)
	{
		return (m_upperLimit[limitIndex] >= m_lowerLimit[limitIndex]);
	}

	const btRigidBody& getRigidBodyA() const
	{
		return m_rbA;
	}
	const btRigidBody& getRigidBodyB() const
	{
		return m_rbB;
	}
	

};

#endif //GENERIC_6DOF_CONSTRAINT_H
