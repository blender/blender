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

#include "TriangleMesh.h"
#include <assert.h>

static int	myindices[3] = {0,1,2};

TriangleMesh::TriangleMesh ()
{

}

void	TriangleMesh::getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& stride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart)
{
	numverts = 3;
	*vertexbase = (unsigned char*)&m_triangles[subpart];
	type = PHY_FLOAT;
	stride = sizeof(SimdVector3);


	numfaces = 1;
	*indexbase = (unsigned char*) &myindices[0];
	indicestype = PHY_INTEGER;
	indexstride = sizeof(int);

}

void	TriangleMesh::getLockedReadOnlyVertexIndexBase(const unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& stride,const unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart) const
{
	numverts = 3;
	*vertexbase = (unsigned char*)&m_triangles[subpart];
	type = PHY_FLOAT;
	stride = sizeof(SimdVector3);


	numfaces = 1;
	*indexbase = (unsigned char*) &myindices[0];
	indicestype = PHY_INTEGER;
	indexstride = sizeof(int);

}



int		TriangleMesh::getNumSubParts() const
{
	return m_triangles.size();
}
