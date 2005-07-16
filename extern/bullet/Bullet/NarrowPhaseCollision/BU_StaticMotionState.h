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

#ifndef BU_STATIC_MOTIONSTATE
#define BU_STATIC_MOTIONSTATE


#include <CollisionShapes/BU_MotionStateInterface.h>

class BU_StaticMotionState :public BU_MotionStateInterface
{
public:
	virtual ~BU_StaticMotionState(){};

	virtual void	SetTransform(const SimdTransform& trans)
	{
		m_trans = trans;
	}
	virtual void	GetTransform(SimdTransform& trans) const
	{
		trans = m_trans;
	}
	virtual void	SetPosition(const SimdPoint3& position)
	{
		m_trans.setOrigin( position );
	}
	virtual void	GetPosition(SimdPoint3& position) const
	{
		position = m_trans.getOrigin();
	}

	virtual void	SetOrientation(const SimdQuaternion& orientation)
	{
		m_trans.setRotation( orientation);
	}
	virtual void	GetOrientation(SimdQuaternion& orientation) const
	{
		orientation = m_trans.getRotation();
	}

	virtual void	SetBasis(const SimdMatrix3x3& basis)
	{
		m_trans.setBasis( basis);
	}
	virtual void	GetBasis(SimdMatrix3x3& basis) const
	{ 
		basis = m_trans.getBasis();
	}

	virtual void	SetLinearVelocity(const SimdVector3& linvel)
	{
		m_linearVelocity = linvel;
	}
	virtual void	GetLinearVelocity(SimdVector3& linvel) const
	{
		linvel = m_linearVelocity;
	}
	
	virtual void	SetAngularVelocity(const SimdVector3& angvel)
	{
		m_angularVelocity = angvel;
	}
	virtual void	GetAngularVelocity(SimdVector3& angvel) const
	{
		angvel = m_angularVelocity;
	}



protected:

	SimdTransform	m_trans;
	SimdVector3		m_angularVelocity;
	SimdVector3		m_linearVelocity;

};

#endif //BU_STATIC_MOTIONSTATE
