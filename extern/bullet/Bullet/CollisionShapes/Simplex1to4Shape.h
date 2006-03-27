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

#ifndef BU_SIMPLEX_1TO4_SHAPE
#define BU_SIMPLEX_1TO4_SHAPE


#include <CollisionShapes/PolyhedralConvexShape.h>
#include "BroadphaseCollision/BroadphaseProxy.h"


///BU_Simplex1to4 implements feature based and implicit simplex of up to 4 vertices (tetrahedron, triangle, line, vertex).
class BU_Simplex1to4 : public PolyhedralConvexShape
{
protected:

	int	m_numVertices;
	SimdPoint3	m_vertices[4];

public:
	BU_Simplex1to4();

	BU_Simplex1to4(const SimdPoint3& pt0);
	BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1);
	BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1,const SimdPoint3& pt2);
	BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1,const SimdPoint3& pt2,const SimdPoint3& pt3);

    
	void	Reset()
	{
		m_numVertices = 0;
	}
	

	virtual int	GetShapeType() const{ return TETRAHEDRAL_SHAPE_PROXYTYPE; }

	void AddVertex(const SimdPoint3& pt);

	//PolyhedralConvexShape interface

	virtual int	GetNumVertices() const;

	virtual int GetNumEdges() const;

	virtual void GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const;
	
	virtual void GetVertex(int i,SimdPoint3& vtx) const;

	virtual int	GetNumPlanes() const;

	virtual void GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i) const;

	virtual int GetIndex(int i) const;

	virtual	bool IsInside(const SimdPoint3& pt,SimdScalar tolerance) const;


	///GetName is for debugging
	virtual  char*	GetName()const { return "BU_Simplex1to4";}

};

#endif //BU_SIMPLEX_1TO4_SHAPE
