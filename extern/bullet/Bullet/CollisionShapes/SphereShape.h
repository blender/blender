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

#ifndef SPHERE_MINKOWSKI_H
#define SPHERE_MINKOWSKI_H

#include "ConvexShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

///SphereShape implements an implicit (getSupportingVertex) Sphere
class SphereShape : public ConvexShape

{
	SimdScalar m_radius;
	
public:
	SphereShape (SimdScalar radius);
	
	
	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec)const;
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;


	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	virtual void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;

	virtual int	GetShapeType() const { return SPHERE_SHAPE_PROXYTYPE; }

	SimdScalar	GetRadius() { return m_radius;}

	//debugging
	virtual char*	GetName()const {return "SPHERE";}

	virtual void	SetMargin(float margin)
	{
		ConvexShape::SetMargin(margin);
	}
	virtual float	GetMargin() const
	{
		//to improve gjk behaviour, use radius+margin as the full margin, so never get into the penetration case
		//this means, non-uniform scaling is not supported anymore
		return m_localScaling[0] * m_radius + ConvexShape::GetMargin();
	}


};


#endif //SPHERE_MINKOWSKI_H
