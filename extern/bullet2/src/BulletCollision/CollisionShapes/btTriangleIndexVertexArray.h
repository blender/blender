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

#ifndef BT_TRIANGLE_INDEX_VERTEX_ARRAY_H
#define BT_TRIANGLE_INDEX_VERTEX_ARRAY_H

#include "btStridingMeshInterface.h"
#include "../../LinearMath/btAlignedObjectArray.h"

///IndexedMesh indexes into existing vertex and index arrays, in a similar way OpenGL glDrawElements
///instead of the number of indices, we pass the number of triangles
///todo: explain with pictures
ATTRIBUTE_ALIGNED16( struct)	btIndexedMesh
{
	int			m_numTriangles;
	const unsigned char *		m_triangleIndexBase;
	int			m_triangleIndexStride;
	int			m_numVertices;
	const unsigned char *		m_vertexBase;
	int			m_vertexStride;
	int			pad[2];
}
;


typedef btAlignedObjectArray<btIndexedMesh>	IndexedMeshArray;

///TriangleIndexVertexArray allows to use multiple meshes, by indexing into existing triangle/index arrays.
///Additional meshes can be added using addIndexedMesh
///No duplcate is made of the vertex/index data, it only indexes into external vertex/index arrays.
///So keep those arrays around during the lifetime of this btTriangleIndexVertexArray.
ATTRIBUTE_ALIGNED16( class) btTriangleIndexVertexArray : public btStridingMeshInterface
{
	IndexedMeshArray	m_indexedMeshes;
	int m_pad[3];

		
public:

	btTriangleIndexVertexArray()
	{
	}

	//just to be backwards compatible
	btTriangleIndexVertexArray(int numTriangleIndices,int* triangleIndexBase,int triangleIndexStride,int numVertices,btScalar* vertexBase,int vertexStride);
	
	void	addIndexedMesh(const btIndexedMesh& mesh)
	{
		m_indexedMeshes.push_back(mesh);
	}
	
	
	virtual void	getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0);

	virtual void	getLockedReadOnlyVertexIndexBase(const unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,const unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0) const;

	/// unLockVertexBase finishes the access to a subpart of the triangle mesh
	/// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
	virtual void	unLockVertexBase(int subpart) {(void)subpart;}

	virtual void	unLockReadOnlyVertexBase(int subpart) const {(void)subpart;}

	/// getNumSubParts returns the number of seperate subparts
	/// each subpart has a continuous array of vertices and indices
	virtual int		getNumSubParts() const { 
		return (int)m_indexedMeshes.size();
	}

	IndexedMeshArray&	getIndexedMeshArray()
	{
		return m_indexedMeshes;
	}

	const IndexedMeshArray&	getIndexedMeshArray() const
	{
		return m_indexedMeshes;
	}

	virtual void	preallocateVertices(int numverts){(void) numverts;}
	virtual void	preallocateIndices(int numindices){(void) numindices;}

}
;

#endif //BT_TRIANGLE_INDEX_VERTEX_ARRAY_H
