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

	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const;


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
