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


#include "btPolyhedralConvexShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h"


///BU_Simplex1to4 implements feature based and implicit simplex of up to 4 vertices (tetrahedron, triangle, line, vertex).
class btBU_Simplex1to4 : public btPolyhedralConvexShape
{
protected:

	int	m_numVertices;
	btPoint3	m_vertices[4];

public:
	btBU_Simplex1to4();

	btBU_Simplex1to4(const btPoint3& pt0);
	btBU_Simplex1to4(const btPoint3& pt0,const btPoint3& pt1);
	btBU_Simplex1to4(const btPoint3& pt0,const btPoint3& pt1,const btPoint3& pt2);
	btBU_Simplex1to4(const btPoint3& pt0,const btPoint3& pt1,const btPoint3& pt2,const btPoint3& pt3);

    
	void	reset()
	{
		m_numVertices = 0;
	}
	

	virtual int	getShapeType() const{ return TETRAHEDRAL_SHAPE_PROXYTYPE; }

	void addVertex(const btPoint3& pt);

	//PolyhedralConvexShape interface

	virtual int	getNumVertices() const;

	virtual int getNumEdges() const;

	virtual void getEdge(int i,btPoint3& pa,btPoint3& pb) const;
	
	virtual void getVertex(int i,btPoint3& vtx) const;

	virtual int	getNumPlanes() const;

	virtual void getPlane(btVector3& planeNormal,btPoint3& planeSupport,int i) const;

	virtual int getIndex(int i) const;

	virtual	bool isInside(const btPoint3& pt,btScalar tolerance) const;


	///getName is for debugging
	virtual const char*	getName()const { return "btBU_Simplex1to4";}

};

#endif //BU_SIMPLEX_1TO4_SHAPE
