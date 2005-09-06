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
#ifndef COLLISION_SHAPE_H
#define COLLISION_SHAPE_H

#include "SimdTransform.h"
#include "SimdVector3.h"
#include <SimdMatrix3x3.h>
#include "SimdPoint3.h"
#include "BroadphaseCollision/BroadphaseProxy.h" //for the shape types

///CollisionShape provides generic interface for collidable objects
class CollisionShape
{

public:

	CollisionShape()
	:m_tempDebug(0)
	{
	}
	virtual ~CollisionShape()
	{
	}

	virtual void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const =0;

	virtual void	GetBoundingSphere(SimdVector3& center,SimdScalar& radius) const;

	virtual float	GetAngularMotionDisc() const;

	virtual int		GetShapeType() const=0;

	///CalculateTemporalAabb calculates the enclosing aabb for the moving object over interval [0..timeStep)
	///result is conservative
	void CalculateTemporalAabb(const SimdTransform& curTrans,const SimdVector3& linvel,const SimdVector3& angvel,SimdScalar timeStep, SimdVector3& temporalAabbMin,SimdVector3& temporalAabbMax);

	bool	IsPolyhedral() const
	{
		return (GetShapeType() < IMPLICIT_CONVEX_SHAPES_START_HERE);
	}

	bool	IsConvex() const
	{
		return (GetShapeType() < CONCAVE_SHAPES_START_HERE);
	}
	bool	IsConcave() const
	{
		return (GetShapeType() > CONCAVE_SHAPES_START_HERE);
	}


	virtual void	setLocalScaling(const SimdVector3& scaling) =0;
	virtual const SimdVector3& getLocalScaling() const =0;

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia) = 0;

//debugging support
	virtual char*	GetName()const =0 ;
	const char* GetExtraDebugInfo() const { return m_tempDebug;}
	void  SetExtraDebugInfo(const char* extraDebugInfo) { m_tempDebug = extraDebugInfo;}
	const char * m_tempDebug;
//endif debugging support

	virtual void	SetMargin(float margin) = 0;
	virtual float	GetMargin() const = 0;

};	

#endif //COLLISION_SHAPE_H

