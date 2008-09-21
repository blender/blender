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

#ifndef CONVEX_HULL_SHAPE_H
#define CONVEX_HULL_SHAPE_H

#include "btPolyhedralConvexShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" // for the types
#include "LinearMath/btAlignedObjectArray.h"

///The btConvexHullShape implements an implicit convex hull of an array of vertices.
///Bullet provides a general and fast collision detector for convex shapes based on GJK and EPA using localGetSupportingVertex.
ATTRIBUTE_ALIGNED16(class) btConvexHullShape : public btPolyhedralConvexShape
{
	btAlignedObjectArray<btPoint3>	m_points;

public:
	BT_DECLARE_ALIGNED_ALLOCATOR();

	
	///this constructor optionally takes in a pointer to points. Each point is assumed to be 3 consecutive btScalar (x,y,z), the striding defines the number of bytes between each point, in memory.
	///It is easier to not pass any points in the constructor, and just add one point at a time, using addPoint.
	///btConvexHullShape make an internal copy of the points.
	btConvexHullShape(const btScalar* points=0,int numPoints=0, int stride=sizeof(btPoint3));

	void addPoint(const btPoint3& point);

	btPoint3* getPoints()
	{
		return &m_points[0];
	}

	const btPoint3* getPoints() const
	{
		return &m_points[0];
	}

	int getNumPoints() const 
	{
		return m_points.size();
	}

	virtual btVector3	localGetSupportingVertex(const btVector3& vec)const;
	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec)const;
	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;
	

	virtual int	getShapeType()const { return CONVEX_HULL_SHAPE_PROXYTYPE; }

	//debugging
	virtual const char*	getName()const {return "Convex";}

	
	virtual int	getNumVertices() const;
	virtual int getNumEdges() const;
	virtual void getEdge(int i,btPoint3& pa,btPoint3& pb) const;
	virtual void getVertex(int i,btPoint3& vtx) const;
	virtual int	getNumPlanes() const;
	virtual void getPlane(btVector3& planeNormal,btPoint3& planeSupport,int i ) const;
	virtual	bool isInside(const btPoint3& pt,btScalar tolerance) const;

	///in case we receive negative scaling
	virtual void	setLocalScaling(const btVector3& scaling);

};


#endif //CONVEX_HULL_SHAPE_H

