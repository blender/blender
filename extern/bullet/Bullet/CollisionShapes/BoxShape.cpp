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

#include "BoxShape.h"

SimdVector3 BoxShape::GetHalfExtents() const
{
	return m_boxHalfExtents1 * m_localScaling;
}
//{ 


void BoxShape::GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	SimdVector3 halfExtents = GetHalfExtents();

	SimdMatrix3x3 abs_b = t.getBasis().absolute();  
	SimdPoint3 center = t.getOrigin();
	SimdVector3 extent = SimdVector3(abs_b[0].dot(halfExtents),
		   abs_b[1].dot(halfExtents),
		  abs_b[2].dot(halfExtents));
	extent += SimdVector3(GetMargin(),GetMargin(),GetMargin());

	aabbMin = center - extent;
	aabbMax = center + extent;


}


void	BoxShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//float margin = 0.f;
	SimdVector3 halfExtents = GetHalfExtents();

	SimdScalar lx=2.f*(halfExtents.x());
	SimdScalar ly=2.f*(halfExtents.y());
	SimdScalar lz=2.f*(halfExtents.z());

	inertia[0] = mass/(12.0f) * (ly*ly + lz*lz);
	inertia[1] = mass/(12.0f) * (lx*lx + lz*lz);
	inertia[2] = mass/(12.0f) * (lx*lx + ly*ly);


}

