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
#ifndef CONE_MINKOWSKI_H
#define CONE_MINKOWSKI_H

#include "ConvexShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

/// implements cone shape interface
class ConeShape : public ConvexShape

{

	float m_sinAngle;
	float m_radius;
	float m_height;
	
	SimdVector3 ConeLocalSupport(const SimdVector3& v) const;


public:
	ConeShape (SimdScalar radius,SimdScalar height);
	
	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec) const;
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec) const;


	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
	{
		SimdTransform identity;
		identity.setIdentity();
		SimdVector3 aabbMin,aabbMax;
		GetAabb(identity,aabbMin,aabbMax);

		SimdVector3 halfExtents = (aabbMax-aabbMin)*0.5f;

		float margin = GetMargin();

		SimdScalar lx=2.f*(halfExtents.x()+margin);
		SimdScalar ly=2.f*(halfExtents.y()+margin);
		SimdScalar lz=2.f*(halfExtents.z()+margin);
		const SimdScalar x2 = lx*lx;
		const SimdScalar y2 = ly*ly;
		const SimdScalar z2 = lz*lz;
		const SimdScalar scaledmass = mass * 0.08333333f;

		inertia = scaledmass * (SimdVector3(y2+z2,x2+z2,x2+y2));

//		inertia.x() = scaledmass * (y2+z2);
//		inertia.y() = scaledmass * (x2+z2);
//		inertia.z() = scaledmass * (x2+y2);
	}



		virtual int	GetShapeType() const { return CONE_SHAPE_PROXYTYPE; }

		virtual char*	GetName()const 
		{
			return "Cone";
		}
};


#endif //CONE_MINKOWSKI_H