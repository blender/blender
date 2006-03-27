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
