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
#ifndef TRIANGLE_MESH_SHAPE_H
#define TRIANGLE_MESH_SHAPE_H

#include "CollisionShapes/CollisionShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

#include "StridingMeshInterface.h"
#include "TriangleCallback.h"



///Concave triangle mesh. Uses an interface to access the triangles to allow for sharing graphics/physics triangles.
class TriangleMeshShape : public CollisionShape
{

	StridingMeshInterface* m_meshInterface;
	float m_collisionMargin;

public:
	TriangleMeshShape(StridingMeshInterface* meshInterface);

	virtual ~TriangleMeshShape();


	
	virtual SimdVector3 LocalGetSupportingVertex(const SimdVector3& vec) const;

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
	{
		assert(0);
		return LocalGetSupportingVertex(vec);
	}

	virtual int	GetShapeType() const
	{
		return TRIANGLE_MESH_SHAPE_PROXYTYPE;
	}

	virtual void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;

	void	ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const;

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	virtual void	setLocalScaling(const SimdVector3& scaling);
	virtual const SimdVector3& getLocalScaling() const;
	

	//debugging
	virtual char*	GetName()const {return "TRIANGLEMESH";}

	
	virtual float GetMargin() const {
		return m_collisionMargin;
	}
	virtual void SetMargin(float collisionMargin)
	{
		m_collisionMargin = collisionMargin;
	}



};

#endif //TRIANGLE_MESH_SHAPE_H
