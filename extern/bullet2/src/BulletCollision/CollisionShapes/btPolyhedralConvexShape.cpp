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

#include <BulletCollision/CollisionShapes/btPolyhedralConvexShape.h>

btPolyhedralConvexShape::btPolyhedralConvexShape()
:m_optionalHull(0)
{

}



btVector3	btPolyhedralConvexShape::localGetSupportingVertexWithoutMargin(const btVector3& vec0)const
{
	int i;
	btVector3 supVec(0,0,0);

	btScalar maxDot(-1e30f);

	btVector3 vec = vec0;
	btScalar lenSqr = vec.length2();
	if (lenSqr < 0.0001f)
	{
		vec.setValue(1,0,0);
	} else
	{
		float rlen = 1.f / btSqrt(lenSqr );
		vec *= rlen;
	}

	btVector3 vtx;
	btScalar newDot;

	for (i=0;i<getNumVertices();i++)
	{
		getVertex(i,vtx);
		newDot = vec.dot(vtx);
		if (newDot > maxDot)
		{
			maxDot = newDot;
			supVec = vtx;
		}
	}

	return supVec;

}

void	btPolyhedralConvexShape::batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const
{
	int i;

	btVector3 vtx;
	btScalar newDot;

	for (i=0;i<numVectors;i++)
	{
		supportVerticesOut[i][3] = -1e30f;
	}

	for (int j=0;j<numVectors;j++)
	{
	
		const btVector3& vec = vectors[j];

		for (i=0;i<getNumVertices();i++)
		{
			getVertex(i,vtx);
			newDot = vec.dot(vtx);
			if (newDot > supportVerticesOut[j][3])
			{
				//WARNING: don't swap next lines, the w component would get overwritten!
				supportVerticesOut[j] = vtx;
				supportVerticesOut[j][3] = newDot;
			}
		}
	}
}



void	btPolyhedralConvexShape::calculateLocalInertia(btScalar mass,btVector3& inertia)
{
	//not yet, return box inertia

	float margin = getMargin();

	btTransform ident;
	ident.setIdentity();
	btVector3 aabbMin,aabbMax;
	getAabb(ident,aabbMin,aabbMax);
	btVector3 halfExtents = (aabbMax-aabbMin)*0.5f;

	btScalar lx=2.f*(halfExtents.x()+margin);
	btScalar ly=2.f*(halfExtents.y()+margin);
	btScalar lz=2.f*(halfExtents.z()+margin);
	const btScalar x2 = lx*lx;
	const btScalar y2 = ly*ly;
	const btScalar z2 = lz*lz;
	const btScalar scaledmass = mass * 0.08333333f;

	inertia = scaledmass * (btVector3(y2+z2,x2+z2,x2+y2));

}

