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
#ifndef MINKOWSKI_SUM_SHAPE_H
#define MINKOWSKI_SUM_SHAPE_H

#include "ConvexShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

/// MinkowskiSumShape represents implicit (getSupportingVertex) based minkowski sum of two convex implicit shapes.
class MinkowskiSumShape : public ConvexShape
{

	SimdTransform	m_transA;
	SimdTransform	m_transB;
	ConvexShape*	m_shapeA;
	ConvexShape*	m_shapeB;

public:

	MinkowskiSumShape(ConvexShape* shapeA,ConvexShape* shapeB);

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	void	SetTransformA(const SimdTransform&	transA) { m_transA = transA;}
	void	SetTransformB(const SimdTransform&	transB) { m_transB = transB;}

	const SimdTransform& GetTransformA()const  { return m_transA;}
	const SimdTransform& GetTransformB()const  { return m_transB;}


	virtual int	GetShapeType() const { return MINKOWSKI_SUM_SHAPE_PROXYTYPE; }

	virtual float	GetMargin() const;

	const ConvexShape*	GetShapeA() const { return m_shapeA;}
	const ConvexShape*	GetShapeB() const { return m_shapeB;}

	virtual char*	GetName()const 
	{
		return "MinkowskiSum";
	}
};

#endif //MINKOWSKI_SUM_SHAPE_H
