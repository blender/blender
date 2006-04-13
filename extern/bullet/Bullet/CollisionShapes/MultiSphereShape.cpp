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

#include "MultiSphereShape.h"
#include "CollisionShapes/CollisionMargin.h"
#include "SimdQuaternion.h"

MultiSphereShape::MultiSphereShape (const SimdVector3& inertiaHalfExtents,const SimdVector3* positions,const SimdScalar* radi,int numSpheres)
:m_inertiaHalfExtents(inertiaHalfExtents)
{
	m_minRadius = 1e30f;

	m_numSpheres = numSpheres;
	for (int i=0;i<m_numSpheres;i++)
	{
		m_localPositions[i] = positions[i];
		m_radi[i] = radi[i];
		if (radi[i] < m_minRadius)
			m_minRadius = radi[i];
	}
	SetMargin(m_minRadius);

}



 
 SimdVector3	MultiSphereShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec0)const
{
	int i;
	SimdVector3 supVec(0,0,0);

	SimdScalar maxDot(-1e30f);


	SimdVector3 vec = vec0;
	SimdScalar lenSqr = vec.length2();
	if (lenSqr < 0.0001f)
	{
		vec.setValue(1,0,0);
	} else
	{
		float rlen = 1.f / SimdSqrt(lenSqr );
		vec *= rlen;
	}

	SimdVector3 vtx;
	SimdScalar newDot;

	const SimdVector3* pos = &m_localPositions[0];
	const SimdScalar* rad = &m_radi[0];

	for (i=0;i<m_numSpheres;i++)
	{
		vtx = (*pos) +vec*((*rad)-m_minRadius);
		pos++;
		rad++;
		newDot = vec.dot(vtx);
		if (newDot > maxDot)
		{
			maxDot = newDot;
			supVec = vtx;
		}
	}

	return supVec;

}

 void	MultiSphereShape::BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
{

	for (int j=0;j<numVectors;j++)
	{
		SimdScalar maxDot(-1e30f);

		const SimdVector3& vec = vectors[j];

		SimdVector3 vtx;
		SimdScalar newDot;

		const SimdVector3* pos = &m_localPositions[0];
		const SimdScalar* rad = &m_radi[0];

		for (int i=0;i<m_numSpheres;i++)
		{
			vtx = (*pos) +vec*((*rad)-m_minRadius);
			pos++;
			rad++;
			newDot = vec.dot(vtx);
			if (newDot > maxDot)
			{
				maxDot = newDot;
				supportVerticesOut[j] = vtx;
			}
		}
	}
}








void	MultiSphereShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//as an approximation, take the inertia of the box that bounds the spheres

	SimdTransform ident;
	ident.setIdentity();
//	SimdVector3 aabbMin,aabbMax;

//	GetAabb(ident,aabbMin,aabbMax);

	SimdVector3 halfExtents = m_inertiaHalfExtents;//(aabbMax - aabbMin)* 0.5f;

	float margin = CONVEX_DISTANCE_MARGIN;

	SimdScalar lx=2.f*(halfExtents[0]+margin);
	SimdScalar ly=2.f*(halfExtents[1]+margin);
	SimdScalar lz=2.f*(halfExtents[2]+margin);
	const SimdScalar x2 = lx*lx;
	const SimdScalar y2 = ly*ly;
	const SimdScalar z2 = lz*lz;
	const SimdScalar scaledmass = mass * 0.08333333f;

	inertia[0] = scaledmass * (y2+z2);
	inertia[1] = scaledmass * (x2+z2);
	inertia[2] = scaledmass * (x2+y2);

}



