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



SimdVector3 CylinderLocalSupportX(const SimdVector3& halfExtents,const SimdVector3& v) 
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

    SimdScalar s = sqrtf(v[XX] * v[XX] + v[ZZ] * v[ZZ]);
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






SimdVector3 CylinderLocalSupportY(const SimdVector3& halfExtents,const SimdVector3& v) 
{

const int cylinderUpAxis = 1;
const int XX = 0;
const int YY = 1;
const int ZZ = 2;


	float radius = halfExtents[XX];
	float halfHeight = halfExtents[cylinderUpAxis];


    SimdVector3 tmp;
	SimdScalar d ;

    SimdScalar s = sqrtf(v[XX] * v[XX] + v[ZZ] * v[ZZ]);
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

SimdVector3 CylinderLocalSupportZ(const SimdVector3& halfExtents,const SimdVector3& v) 
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

    SimdScalar s = sqrtf(v[XX] * v[XX] + v[ZZ] * v[ZZ]);
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







