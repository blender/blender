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

#include "btMultiSphereShape.h"
#include "BulletCollision/CollisionShapes/btCollisionMargin.h"
#include "LinearMath/btQuaternion.h"

btMultiSphereShape::btMultiSphereShape (const btVector3& inertiaHalfExtents,const btVector3* positions,const btScalar* radi,int numSpheres)
:btConvexInternalShape (), m_inertiaHalfExtents(inertiaHalfExtents)
{
	m_shapeType = MULTI_SPHERE_SHAPE_PROXYTYPE;
	btScalar startMargin = btScalar(1e30);

	m_numSpheres = numSpheres;
	for (int i=0;i<m_numSpheres;i++)
	{
		m_localPositions[i] = positions[i];
		m_radi[i] = radi[i];
		if (radi[i] < startMargin)
			startMargin = radi[i];
	}
	setMargin(startMargin);

}



 
 btVector3	btMultiSphereShape::localGetSupportingVertexWithoutMargin(const btVector3& vec0)const
{
	int i;
	btVector3 supVec(0,0,0);

	btScalar maxDot(btScalar(-1e30));


	btVector3 vec = vec0;
	btScalar lenSqr = vec.length2();
	if (lenSqr < (SIMD_EPSILON*SIMD_EPSILON))
	{
		vec.setValue(1,0,0);
	} else
	{
		btScalar rlen = btScalar(1.) / btSqrt(lenSqr );
		vec *= rlen;
	}

	btVector3 vtx;
	btScalar newDot;

	const btVector3* pos = &m_localPositions[0];
	const btScalar* rad = &m_radi[0];

	for (i=0;i<m_numSpheres;i++)
	{
		vtx = (*pos) +vec*m_localScaling*(*rad) - vec * getMargin();
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

 void	btMultiSphereShape::batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const
{

	for (int j=0;j<numVectors;j++)
	{
		btScalar maxDot(btScalar(-1e30));

		const btVector3& vec = vectors[j];

		btVector3 vtx;
		btScalar newDot;

		const btVector3* pos = &m_localPositions[0];
		const btScalar* rad = &m_radi[0];

		for (int i=0;i<m_numSpheres;i++)
		{
			vtx = (*pos) +vec*m_localScaling*(*rad) - vec * getMargin();
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








void	btMultiSphereShape::calculateLocalInertia(btScalar mass,btVector3& inertia) const
{
	//as an approximation, take the inertia of the box that bounds the spheres

	btTransform ident;
	ident.setIdentity();
//	btVector3 aabbMin,aabbMax;

//	getAabb(ident,aabbMin,aabbMax);

	btVector3 halfExtents = m_inertiaHalfExtents;//(aabbMax - aabbMin)* btScalar(0.5);

	btScalar margin = CONVEX_DISTANCE_MARGIN;

	btScalar lx=btScalar(2.)*(halfExtents[0]+margin);
	btScalar ly=btScalar(2.)*(halfExtents[1]+margin);
	btScalar lz=btScalar(2.)*(halfExtents[2]+margin);
	const btScalar x2 = lx*lx;
	const btScalar y2 = ly*ly;
	const btScalar z2 = lz*lz;
	const btScalar scaledmass = mass * btScalar(.08333333);

	inertia[0] = scaledmass * (y2+z2);
	inertia[1] = scaledmass * (x2+z2);
	inertia[2] = scaledmass * (x2+y2);

}



