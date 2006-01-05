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

#include "StridingMeshInterface.h"


class TriangleIndexVertexArray : public StridingMeshInterface
{

	int			m_numTriangleIndices;
	int*		m_triangleIndexBase;
	int			m_triangleIndexStride;
	int			m_numVertices;
	float*		m_vertexBase;
	int			m_vertexStride;
	
public:

	TriangleIndexVertexArray(int numTriangleIndices,int* triangleIndexBase,int triangleIndexStride,int numVertices,float* vertexBase,int vertexStride);
	
	virtual void	getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0);

	virtual void	getLockedReadOnlyVertexIndexBase(const unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,const unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0) const;

	/// unLockVertexBase finishes the access to a subpart of the triangle mesh
	/// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
	virtual void	unLockVertexBase(int subpart) {}

	virtual void	unLockReadOnlyVertexBase(int subpart) const {}

	/// getNumSubParts returns the number of seperate subparts
	/// each subpart has a continuous array of vertices and indices
	virtual int		getNumSubParts() const { return 1;}
	
	virtual void	preallocateVertices(int numverts){}
	virtual void	preallocateIndices(int numindices){}

};

