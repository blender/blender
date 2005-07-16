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

int		TriangleMesh::getNumSubParts()
{
	return m_triangles.size();
}
