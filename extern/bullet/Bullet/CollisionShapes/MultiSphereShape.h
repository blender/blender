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
#ifndef MULTI_SPHERE_MINKOWSKI_H
#define MULTI_SPHERE_MINKOWSKI_H

#include "ConvexShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

#define MAX_NUM_SPHERES 5

///MultiSphereShape represents implicit convex hull of a collection of spheres (using getSupportingVertex)
class MultiSphereShape : public ConvexShape

{
	
	SimdVector3 m_localPositions[MAX_NUM_SPHERES];
	SimdScalar  m_radi[MAX_NUM_SPHERES];
	SimdVector3	m_inertiaHalfExtents;

	int m_numSpheres;
	float m_minRadius;





public:
	MultiSphereShape (const SimdVector3& inertiaHalfExtents,const SimdVector3* positions,const SimdScalar* radi,int numSpheres);

	///CollisionShape Interface
	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	/// ConvexShape Interface
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;


	virtual int	GetShapeType() const { return MULTI_SPHERE_SHAPE_PROXYTYPE; }

	virtual char*	GetName()const 
	{
		return "MultiSphere";
	}

};


#endif //MULTI_SPHERE_MINKOWSKI_H
