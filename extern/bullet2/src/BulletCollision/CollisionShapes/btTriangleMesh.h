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


#ifndef TRIANGLE_MESH_H
#define TRIANGLE_MESH_H

#include "btStridingMeshInterface.h"
#include "../../LinearMath/btVector3.h"
#include "../../LinearMath/btAlignedObjectArray.h"

///TriangleMesh provides storage for a concave triangle mesh. It can be used as data for the btTriangleMeshShape.
class btTriangleMesh : public btStridingMeshInterface
{
	btAlignedObjectArray<btVector3>	m_vertices;
	btAlignedObjectArray<int>		m_indices;

	public:
		btTriangleMesh ();

		void	addTriangle(const btVector3& vertex0,const btVector3& vertex1,const btVector3& vertex2)
		{
			int curIndex = m_indices.size();
			m_vertices.push_back(vertex0);
			m_vertices.push_back(vertex1);
			m_vertices.push_back(vertex2);

			m_indices.push_back(curIndex++);
			m_indices.push_back(curIndex++);
			m_indices.push_back(curIndex++);
		}

		int getNumTriangles() const
		{
			return m_indices.size() / 3;
		}

		

//StridingMeshInterface interface implementation

		virtual void	getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& stride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0);

		virtual void	getLockedReadOnlyVertexIndexBase(const unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& stride,const unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0) const;

		/// unLockVertexBase finishes the access to a subpart of the triangle mesh
		/// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
		virtual void	unLockVertexBase(int subpart) {(void) subpart;}

		virtual void	unLockReadOnlyVertexBase(int subpart) const { (void) subpart;}

		/// getNumSubParts returns the number of seperate subparts
		/// each subpart has a continuous array of vertices and indices
		virtual int		getNumSubParts() const;
		
		virtual void	preallocateVertices(int numverts){(void) numverts;}
		virtual void	preallocateIndices(int numindices){(void) numindices;}

		
};

#endif //TRIANGLE_MESH_H

