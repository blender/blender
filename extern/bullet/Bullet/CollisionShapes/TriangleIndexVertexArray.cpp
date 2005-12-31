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

#include "TriangleIndexVertexArray.h"

TriangleIndexVertexArray::TriangleIndexVertexArray(int numTriangleIndices,int* triangleIndexBase,int triangleIndexStride,int numVertices,float* vertexBase,int vertexStride)
:m_numTriangleIndices(numTriangleIndices),
m_triangleIndexBase(triangleIndexBase),
m_triangleIndexStride(triangleIndexStride),
m_numVertices(numVertices),
m_vertexBase(vertexBase),
m_vertexStride(vertexStride)
{
}

void	TriangleIndexVertexArray::getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart)
{
	numverts = m_numVertices;
	(*vertexbase) = (unsigned char *)m_vertexBase;
	type = PHY_FLOAT;
	vertexStride = m_vertexStride;

	numfaces = m_numTriangleIndices;
	(*indexbase) = (unsigned char *)m_triangleIndexBase;
	indexstride = m_triangleIndexStride;
	indicestype = PHY_INTEGER;
}

void	TriangleIndexVertexArray::getLockedReadOnlyVertexIndexBase(const unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,const unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart) const
{
	numverts = m_numVertices;
	(*vertexbase) = (unsigned char *)m_vertexBase;
	type = PHY_FLOAT;
	vertexStride = m_vertexStride;

	numfaces = m_numTriangleIndices;
	(*indexbase) = (unsigned char *)m_triangleIndexBase;
	indexstride = m_triangleIndexStride;
	indicestype = PHY_INTEGER;
}

