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

#ifndef TRIANGLE_MESH_SHAPE_H
#define TRIANGLE_MESH_SHAPE_H

#include "CollisionShapes/ConcaveShape.h"
#include "CollisionShapes/StridingMeshInterface.h"


///Concave triangle mesh. Uses an interface to access the triangles to allow for sharing graphics/physics triangles.
class TriangleMeshShape : public ConcaveShape
{
protected:
	StridingMeshInterface* m_meshInterface;
	SimdVector3	m_localAabbMin;
	SimdVector3	m_localAabbMax;
	

public:
	TriangleMeshShape(StridingMeshInterface* meshInterface);

	virtual ~TriangleMeshShape();

	virtual SimdVector3 LocalGetSupportingVertex(const SimdVector3& vec) const;

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
	{
		assert(0);
		return LocalGetSupportingVertex(vec);
	}

	void	RecalcLocalAabb();

	virtual int	GetShapeType() const
	{
		return TRIANGLE_MESH_SHAPE_PROXYTYPE;
	}

	virtual void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;

	virtual void	ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const;

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	virtual void	setLocalScaling(const SimdVector3& scaling);
	virtual const SimdVector3& getLocalScaling() const;
	

	//debugging
	virtual char*	GetName()const {return "TRIANGLEMESH";}


};

#endif //TRIANGLE_MESH_SHAPE_H
