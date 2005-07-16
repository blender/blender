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

#ifndef BU_COLLIDABLE
#define BU_COLLIDABLE


class PolyhedralConvexShape;
class BU_MotionStateInterface;
#include <SimdPoint3.h>

class BU_Collidable
{
public:
	BU_Collidable(BU_MotionStateInterface& motion,PolyhedralConvexShape& shape, void* userPointer);

	void*		GetUserPointer() const
	{
		return m_userPointer;
	}

	BU_MotionStateInterface&	GetMotionState()
	{
		return m_motionState;
	}
	inline const BU_MotionStateInterface&	GetMotionState() const
	{
		return m_motionState;
	}
	
	inline const PolyhedralConvexShape&	GetShape() const
	{
		return m_shape;
	};


private:
	BU_MotionStateInterface& m_motionState;
	PolyhedralConvexShape&	m_shape;
	void*		m_userPointer;

};

#endif //BU_COLLIDABLE
