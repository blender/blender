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

void	MinkowskiSumShape::BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
{
	//todo: could make recursive use of batching. probably this shape is not used frequently.
	for (int i=0;i<numVectors;i++)
	{
		supportVerticesOut[i] = LocalGetSupportingVertexWithoutMargin(vectors[i]);
	}

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
