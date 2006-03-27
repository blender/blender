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

