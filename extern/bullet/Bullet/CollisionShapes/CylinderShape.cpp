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
#include "CylinderShape.h"
#include "SimdPoint3.h"

CylinderShape::CylinderShape (const SimdVector3& halfExtents)
:BoxShape(halfExtents)
{

}


CylinderShapeX::CylinderShapeX (const SimdVector3& halfExtents)
:CylinderShape(halfExtents)
{
}


CylinderShapeZ::CylinderShapeZ (const SimdVector3& halfExtents)
:CylinderShape(halfExtents)
{
}



inline SimdVector3 CylinderLocalSupportX(const SimdVector3& halfExtents,const SimdVector3& v) 
{
const int cylinderUpAxis = 0;
const int XX = 1;
const int YY = 0;
const int ZZ = 2;

	//mapping depends on how cylinder local orientation is
	// extents of the cylinder is: X,Y is for radius, and Z for height


	float radius = halfExtents[XX];
	float halfHeight = halfExtents[cylinderUpAxis];


    SimdVector3 tmp;
	SimdScalar d ;

    SimdScalar s = SimdSqrt(v[XX] * v[XX] + v[ZZ] * v[ZZ]);
    if (s != SimdScalar(0.0))
	{
        d = radius / s;  
		tmp[XX] = v[XX] * d;
		tmp[YY] = v[YY] < 0.0 ? -halfHeight : halfHeight;
		tmp[ZZ] = v[ZZ] * d;
		return tmp;
	}
    else
	{
	    tmp[XX] = radius;
		tmp[YY] = v[YY] < 0.0 ? -halfHeight : halfHeight;
		tmp[ZZ] = SimdScalar(0.0);
		return tmp;
    }


}






inline  SimdVector3 CylinderLocalSupportY(const SimdVector3& halfExtents,const SimdVector3& v) 
{

const int cylinderUpAxis = 1;
const int XX = 0;
const int YY = 1;
const int ZZ = 2;


	float radius = halfExtents[XX];
	float halfHeight = halfExtents[cylinderUpAxis];


    SimdVector3 tmp;
	SimdScalar d ;

    SimdScalar s = SimdSqrt(v[XX] * v[XX] + v[ZZ] * v[ZZ]);
    if (s != SimdScalar(0.0))
	{
        d = radius / s;  
		tmp[XX] = v[XX] * d;
		tmp[YY] = v[YY] < 0.0 ? -halfHeight : halfHeight;
		tmp[ZZ] = v[ZZ] * d;
		return tmp;
	}
    else
	{
	    tmp[XX] = radius;
		tmp[YY] = v[YY] < 0.0 ? -halfHeight : halfHeight;
		tmp[ZZ] = SimdScalar(0.0);
		return tmp;
    }

}

inline SimdVector3 CylinderLocalSupportZ(const SimdVector3& halfExtents,const SimdVector3& v) 
{
const int cylinderUpAxis = 2;
const int XX = 0;
const int YY = 2;
const int ZZ = 1;

	//mapping depends on how cylinder local orientation is
	// extents of the cylinder is: X,Y is for radius, and Z for height


	float radius = halfExtents[XX];
	float halfHeight = halfExtents[cylinderUpAxis];


    SimdVector3 tmp;
	SimdScalar d ;

    SimdScalar s = SimdSqrt(v[XX] * v[XX] + v[ZZ] * v[ZZ]);
    if (s != SimdScalar(0.0))
	{
        d = radius / s;  
		tmp[XX] = v[XX] * d;
		tmp[YY] = v[YY] < 0.0 ? -halfHeight : halfHeight;
		tmp[ZZ] = v[ZZ] * d;
		return tmp;
	}
    else
	{
	    tmp[XX] = radius;
		tmp[YY] = v[YY] < 0.0 ? -halfHeight : halfHeight;
		tmp[ZZ] = SimdScalar(0.0);
		return tmp;
    }


}

SimdVector3	CylinderShapeX::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
{
	return CylinderLocalSupportX(GetHalfExtents(),vec);
}


SimdVector3	CylinderShapeZ::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
{
	return CylinderLocalSupportZ(GetHalfExtents(),vec);
}
SimdVector3	CylinderShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
{
	return CylinderLocalSupportY(GetHalfExtents(),vec);
}

void	CylinderShape::BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
{
	for (int i=0;i<numVectors;i++)
	{
		supportVerticesOut[i] = CylinderLocalSupportY(GetHalfExtents(),vectors[i]);
	}
}

void	CylinderShapeZ::BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
{
	for (int i=0;i<numVectors;i++)
	{
		supportVerticesOut[i] = CylinderLocalSupportZ(GetHalfExtents(),vectors[i]);
	}
}




void	CylinderShapeX::BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const
{
	for (int i=0;i<numVectors;i++)
	{
		supportVerticesOut[i] = CylinderLocalSupportX(GetHalfExtents(),vectors[i]);
	}
}


