/*
 * Copyright (c) 2006 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#ifndef TYPED_CONSTRAINT_H
#define TYPED_CONSTRAINT_H



//TypedConstraint is the baseclass for Bullet constraints and vehicles
class TypedConstraint
{
	int	m_userConstraintType;
	int	m_userConstraintId;

public:

	TypedConstraint()
		: m_userConstraintId(-1),
m_userConstraintType(-1)
	{

	}

	int GetUserConstraintType() const
	{
		return m_userConstraintType ;
	}

	void	SetUserConstraintType(int userConstraintType)
	{
		m_userConstraintType = userConstraintType;
	};

	void	SetUserConstraintId(int uid)
	{
		m_userConstraintId = uid;
	}
	
	int GetUserConstraintId()
	{
		return m_userConstraintId;
	}
};

#endif //TYPED_CONSTRAINT_H