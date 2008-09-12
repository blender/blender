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
/*
2007-09-09
Added support for typed joints by Francisco Le?n
email: projectileman@yahoo.com
http://gimpact.sf.net
*/

#ifndef TYPED_JOINT_H
#define TYPED_JOINT_H

#include "btOdeJoint.h"
#include "BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h"
#include "BulletDynamics/ConstraintSolver/btGeneric6DofConstraint.h"
#include "BulletDynamics/ConstraintSolver/btSliderConstraint.h"

struct btOdeSolverBody;

class btOdeTypedJoint : public btOdeJoint
{
public:
	btTypedConstraint * m_constraint;
	int		m_index;
	bool	m_swapBodies;
	btOdeSolverBody* m_body0;
	btOdeSolverBody* m_body1;

	btOdeTypedJoint(){}
	btOdeTypedJoint(
		btTypedConstraint * constraint,
		int index,bool swap,btOdeSolverBody* body0,btOdeSolverBody* body1):
			m_constraint(constraint),
			m_index(index),
			m_swapBodies(swap),
			m_body0(body0),
			m_body1(body1)
	{
	}

	virtual void GetInfo1(Info1 *info);
	virtual void GetInfo2(Info2 *info);
};



class OdeP2PJoint : public btOdeTypedJoint
{
protected:
	inline btPoint2PointConstraint * getP2PConstraint()
	{
		return static_cast<btPoint2PointConstraint * >(m_constraint);
	}
public:

	OdeP2PJoint() {};

	OdeP2PJoint(btTypedConstraint* constraint,int index,bool swap,btOdeSolverBody* body0,btOdeSolverBody* body1);

	//btOdeJoint interface for solver

	virtual void GetInfo1(Info1 *info);

	virtual void GetInfo2(Info2 *info);
};


class OdeD6Joint : public btOdeTypedJoint
{
protected:
	inline btGeneric6DofConstraint * getD6Constraint()
	{
		return static_cast<btGeneric6DofConstraint * >(m_constraint);
	}

	int setLinearLimits(Info2 *info);
	int setAngularLimits(Info2 *info, int row_offset);

public:

	OdeD6Joint() {};

	OdeD6Joint(btTypedConstraint* constraint,int index,bool swap,btOdeSolverBody* body0,btOdeSolverBody* body1);

	//btOdeJoint interface for solver

	virtual void GetInfo1(Info1 *info);

	virtual void GetInfo2(Info2 *info);
};

//! retrieves the constraint info from a btRotationalLimitMotor object
/*! \pre testLimitValue must be called on limot*/
int bt_get_limit_motor_info2(
	btRotationalLimitMotor * limot,
	btRigidBody * body0, btRigidBody * body1,
	btOdeJoint::Info2 *info, int row, btVector3& ax1, int rotational);

/*
OdeSliderJoint
Ported from ODE by Roman Ponomarev (rponom@gmail.com)
April 24, 2008
*/
class OdeSliderJoint : public btOdeTypedJoint
{
protected:
	inline btSliderConstraint * getSliderConstraint()
	{
		return static_cast<btSliderConstraint * >(m_constraint);
	}
public:

	OdeSliderJoint() {};

	OdeSliderJoint(btTypedConstraint* constraint,int index,bool swap, btOdeSolverBody* body0, btOdeSolverBody* body1);

	//BU_Joint interface for solver

	virtual void GetInfo1(Info1 *info);

	virtual void GetInfo2(Info2 *info);
};




#endif //CONTACT_JOINT_H



