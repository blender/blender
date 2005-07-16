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
#include "MinkowskiSumShape.h"


MinkowskiSumShape::MinkowskiSumShape(ConvexShape* shapeA,ConvexShape* shapeB)
:m_shapeA(shapeA),
m_shapeB(shapeB)
{
	m_transA.setIdentity();
	m_transB.setIdentity();
}

SimdVector3 MinkowskiSumShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
{
	SimdVector3 supVertexA = m_transA(m_shapeA->LocalGetSupportingVertexWithoutMargin(vec*m_transA.getBasis()));
	SimdVector3 supVertexB = m_transB(m_shapeB->LocalGetSupportingVertexWithoutMargin(vec*m_transB.getBasis()));
	return supVertexA + supVertexB;
}

float	MinkowskiSumShape::GetMargin() const
{
	return m_shapeA->GetMargin() + m_shapeB->GetMargin();
}


void	MinkowskiSumShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	assert(0);
	inertia.setValue(0,0,0);
}
