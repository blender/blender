/*
 * Copyright (c) 2006 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#ifndef BVH_TRIANGLE_MESH_SHAPE_H
#define BVH_TRIANGLE_MESH_SHAPE_H

#include "CollisionShapes/TriangleMeshShape.h"
#include "CollisionShapes/OptimizedBvh.h"

///Bvh Concave triangle mesh is a static-triangle mesh shape with Bounding Volume Hierarchy optimization.
///Uses an interface to access the triangles to allow for sharing graphics/physics triangles.
class BvhTriangleMeshShape : public TriangleMeshShape
{

	OptimizedBvh*	m_bvh;
	
	
public:
	BvhTriangleMeshShape(StridingMeshInterface* meshInterface);

	virtual ~BvhTriangleMeshShape();

	
	/*
	virtual int	GetShapeType() const
	{
		return TRIANGLE_MESH_SHAPE_PROXYTYPE;
	}
	*/



	virtual void	ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const;


	//debugging
	virtual char*	GetName()const {return "BVHTRIANGLEMESH";}


	virtual void	setLocalScaling(const SimdVector3& scaling);
	


};

#endif //BVH_TRIANGLE_MESH_SHAPE_H
