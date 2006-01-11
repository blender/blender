/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#include <CollisionShapes/PolyhedralConvexShape.h>

PolyhedralConvexShape::PolyhedralConvexShape()

{
}

SimdVector3	PolyhedralConvexShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec0)const
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

	for (i=0;i<GetNumVertices();i++)
	{
		GetVertex(i,vtx);
		newDot = vec.dot(vtx);
		if (newDot > maxDot)
		{
			maxDot = newDot;
			supVec = vtx;
		}
	}

	return supVec;

}

void	PolyhedralConvexShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//not yet, return box inertia

	float margin = GetMargin();

	SimdTransform ident;
	ident.setIdentity();
	SimdVector3 aabbMin,aabbMax;
	GetAabb(ident,aabbMin,aabbMax);
	SimdVector3 halfExtents = (aabbMax-aabbMin)*0.5f;

	SimdScalar lx=2.f*(halfExtents.x()+margin);
	SimdScalar ly=2.f*(halfExtents.y()+margin);
	SimdScalar lz=2.f*(halfExtents.z()+margin);
	const SimdScalar x2 = lx*lx;
	const SimdScalar y2 = ly*ly;
	const SimdScalar z2 = lz*lz;
	const SimdScalar scaledmass = mass * 0.08333333f;

	inertia = scaledmass * (SimdVector3(y2+z2,x2+z2,x2+y2));

}

