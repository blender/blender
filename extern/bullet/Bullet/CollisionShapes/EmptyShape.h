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

#ifndef EMPTY_SHAPE_H
#define EMPTY_SHAPE_H

#include "CollisionShape.h"

#include "SimdVector3.h"
#include "SimdTransform.h"
#include "SimdMatrix3x3.h"
#include <vector>
#include "CollisionShapes/CollisionMargin.h"




/// EmptyShape is a collision shape without actual collision detection. It can be replaced by another shape during runtime
class EmptyShape	: public CollisionShape
{
public:
	EmptyShape();

	virtual ~EmptyShape();


	///GetAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;


	virtual void	setLocalScaling(const SimdVector3& scaling)
	{
		m_localScaling = scaling;
	}
	virtual const SimdVector3& getLocalScaling() const 
	{
		return m_localScaling;
	}

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);
	
	virtual int	GetShapeType() const { return EMPTY_SHAPE_PROXYTYPE;}

	virtual void	SetMargin(float margin)
	{
		m_collisionMargin = margin;
	}
	virtual float	GetMargin() const
	{
		return m_collisionMargin;
	}
	virtual char*	GetName()const
	{
		return "Empty";
	}


private:
	SimdScalar	m_collisionMargin;
protected:
	SimdVector3	m_localScaling;

};



#endif //EMPTY_SHAPE_H
