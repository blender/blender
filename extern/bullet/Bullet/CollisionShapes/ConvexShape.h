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

#ifndef CONVEX_SHAPE_INTERFACE1
#define CONVEX_SHAPE_INTERFACE1

#include "CollisionShape.h"

#include "SimdVector3.h"
#include "SimdTransform.h"
#include "SimdMatrix3x3.h"
#include <vector>
#include "NarrowPhaseCollision/CollisionMargin.h"

//todo: get rid of this ConvexCastResult thing!
struct ConvexCastResult;


/// ConvexShape is an abstract shape interface.
/// The explicit part provides plane-equations, the implicit part provides GetClosestPoint interface.
/// used in combination with GJK or ConvexCast
class ConvexShape : public CollisionShape
{
public:
	ConvexShape();

	virtual ~ConvexShape();

	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec)const;
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec) const= 0;

	// testing for hullnode code

	///GetAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
	{
		GetAabbSlow(t,aabbMin,aabbMax);
	}


	
	virtual void GetAabbSlow(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;


	virtual void	setLocalScaling(const SimdVector3& scaling);
	virtual const SimdVector3& getLocalScaling() const 
	{
		return m_localScaling;
	}


	virtual void	SetMargin(float margin)
	{
		m_collisionMargin = margin;
	}
	virtual float	GetMargin() const
	{
		return m_collisionMargin;
	}
private:
	SimdScalar	m_collisionMargin;
	//local scaling. collisionMargin is not scaled !
protected:
	SimdVector3	m_localScaling;

};



#endif //CONVEX_SHAPE_INTERFACE1
