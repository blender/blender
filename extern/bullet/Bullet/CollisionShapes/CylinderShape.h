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

#ifndef CYLINDER_MINKOWSKI_H
#define CYLINDER_MINKOWSKI_H

#include "BoxShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types
#include "SimdVector3.h"

/// implements cylinder shape interface
class CylinderShape : public BoxShape

{

public:
	CylinderShape (const SimdVector3& halfExtents);
	

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;

	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec) const
	{

		SimdVector3 supVertex;
		supVertex = LocalGetSupportingVertexWithoutMargin(vec);
		
		if ( GetMargin()!=0.f )
		{
			SimdVector3 vecnorm = vec;
			if (vecnorm .length2() == 0.f)
			{
				vecnorm.setValue(-1.f,-1.f,-1.f);
			} 
			vecnorm.normalize();
			supVertex+= GetMargin() * vecnorm;
		}
		return supVertex;
	}


	//use box inertia
	//	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	virtual int	GetShapeType() const
	{
		return CYLINDER_SHAPE_PROXYTYPE;
	}
	


};

class CylinderShapeX : public CylinderShape
{
public:
	CylinderShapeX (const SimdVector3& halfExtents);

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;
};

class CylinderShapeZ : public CylinderShape
{
public:
	CylinderShapeZ (const SimdVector3& halfExtents);

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;

};


#endif //CYLINDER_MINKOWSKI_H