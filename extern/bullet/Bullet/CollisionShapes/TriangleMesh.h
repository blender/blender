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

#ifndef TRIANGLE_MESH_H
#define TRIANGLE_MESH_H

#include "CollisionShapes/StridingMeshInterface.h"
#include <vector>
#include <SimdVector3.h>

struct MyTriangle
{
	SimdVector3	m_vert0;
	SimdVector3	m_vert1;
	SimdVector3	m_vert2;
};

///TriangleMesh provides storage for a concave triangle mesh. It can be used as data for the TriangleMeshShape.
class TriangleMesh : public StridingMeshInterface
{
	std::vector<MyTriangle>	m_triangles;

	public:
		TriangleMesh ();

		void	AddTriangle(const SimdVector3& vertex0,const SimdVector3& vertex1,const SimdVector3& vertex2)
		{
			MyTriangle tri;
			tri.m_vert0 = vertex0;
			tri.m_vert1 = vertex1;
			tri.m_vert2 = vertex2;
			m_triangles.push_back(tri);
		}


//StridingMeshInterface interface implementation

		virtual void	getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& stride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0);

		/// unLockVertexBase finishes the access to a subpart of the triangle mesh
		/// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
		virtual void	unLockVertexBase(int subpart) {}

		/// getNumSubParts returns the number of seperate subparts
		/// each subpart has a continuous array of vertices and indices
		virtual int		getNumSubParts();
		
		virtual void	preallocateVertices(int numverts){}
		virtual void	preallocateIndices(int numindices){}



};

#endif //TRIANGLE_MESH_H