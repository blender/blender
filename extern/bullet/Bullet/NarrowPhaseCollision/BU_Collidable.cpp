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

#include "BU_Collidable.h"
#include "CollisionShapes/CollisionShape.h"
#include <SimdTransform.h>
#include "BU_MotionStateInterface.h"

BU_Collidable::BU_Collidable(BU_MotionStateInterface& motion,PolyhedralConvexShape& shape,void* userPointer )
:m_motionState(motion),m_shape(shape),m_userPointer(userPointer)
{
}
