/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
 */

#include "EmptyShape.h"


#include "CollisionShape.h"


EmptyShape::EmptyShape()
{
}


EmptyShape::~EmptyShape()
{
}


	///GetAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
void EmptyShape::GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	SimdVector3 margin(GetMargin(),GetMargin(),GetMargin());

	aabbMin = t.getOrigin() - margin;

	aabbMax = t.getOrigin() + margin;

}

void	EmptyShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	assert(0);
}

	
	
