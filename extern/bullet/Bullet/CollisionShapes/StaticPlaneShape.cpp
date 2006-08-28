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

#include "StaticPlaneShape.h"

#include "SimdTransformUtil.h"


StaticPlaneShape::StaticPlaneShape(const SimdVector3& planeNormal,SimdScalar planeConstant)
:m_planeNormal(planeNormal),
m_planeConstant(planeConstant),
m_localScaling(0.f,0.f,0.f)
{
}


StaticPlaneShape::~StaticPlaneShape()
{
}



void StaticPlaneShape::GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{
	SimdVector3 infvec (1e30f,1e30f,1e30f);

	SimdVector3 center = m_planeNormal*m_planeConstant;
	aabbMin = center + infvec*m_planeNormal;
	aabbMax = aabbMin;
	aabbMin.setMin(center - infvec*m_planeNormal);
	aabbMax.setMax(center - infvec*m_planeNormal); 

	aabbMin.setValue(-1e30f,-1e30f,-1e30f);
	aabbMax.setValue(1e30f,1e30f,1e30f);

}




void	StaticPlaneShape::ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{

	SimdVector3 halfExtents = (aabbMax - aabbMin) * 0.5f;
	SimdScalar radius = halfExtents.length();
	SimdVector3 center = (aabbMax + aabbMin) * 0.5f;
	
	//this is where the triangles are generated, given AABB and plane equation (normal/constant)

	SimdVector3 tangentDir0,tangentDir1;

	//tangentDir0/tangentDir1 can be precalculated
	SimdPlaneSpace1(m_planeNormal,tangentDir0,tangentDir1);

	SimdVector3 supVertex0,supVertex1;

	SimdVector3 projectedCenter = center - (m_planeNormal.dot(center) - m_planeConstant)*m_planeNormal;
	
	SimdVector3 triangle[3];
	triangle[0] = projectedCenter + tangentDir0*radius + tangentDir1*radius;
	triangle[1] = projectedCenter + tangentDir0*radius - tangentDir1*radius;
	triangle[2] = projectedCenter - tangentDir0*radius - tangentDir1*radius;

	callback->ProcessTriangle(triangle,0,0);

	triangle[0] = projectedCenter - tangentDir0*radius - tangentDir1*radius;
	triangle[1] = projectedCenter - tangentDir0*radius + tangentDir1*radius;
	triangle[2] = projectedCenter + tangentDir0*radius + tangentDir1*radius;

	callback->ProcessTriangle(triangle,0,1);

}

void	StaticPlaneShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//moving concave objects not supported
	
	inertia.setValue(0.f,0.f,0.f);
}

void	StaticPlaneShape::setLocalScaling(const SimdVector3& scaling)
{
	m_localScaling = scaling;
}
const SimdVector3& StaticPlaneShape::getLocalScaling() const
{
	return m_localScaling;
}
