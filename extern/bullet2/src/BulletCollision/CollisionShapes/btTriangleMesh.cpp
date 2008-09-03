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

#include "btTriangleMesh.h"



btTriangleMesh::btTriangleMesh (bool use32bitIndices,bool use4componentVertices)
:m_use32bitIndices(use32bitIndices),
m_use4componentVertices(use4componentVertices)
{
	btIndexedMesh meshIndex;
	meshIndex.m_numTriangles = 0;
	meshIndex.m_numVertices = 0;
	meshIndex.m_indexType = PHY_INTEGER;
	meshIndex.m_triangleIndexBase = 0;
	meshIndex.m_triangleIndexStride = 3*sizeof(int);
	meshIndex.m_vertexBase = 0;
	meshIndex.m_vertexStride = sizeof(btVector3);
	m_indexedMeshes.push_back(meshIndex);

	if (m_use32bitIndices)
	{
		m_indexedMeshes[0].m_numTriangles = m_32bitIndices.size()/3;
		m_indexedMeshes[0].m_triangleIndexBase = (unsigned char*) &m_32bitIndices[0];
		m_indexedMeshes[0].m_indexType = PHY_INTEGER;
		m_indexedMeshes[0].m_triangleIndexStride = 3*sizeof(int);
	} else
	{
		m_indexedMeshes[0].m_numTriangles = m_16bitIndices.size()/3;
		m_indexedMeshes[0].m_triangleIndexBase = (unsigned char*) &m_16bitIndices[0];
		m_indexedMeshes[0].m_indexType = PHY_SHORT;
		m_indexedMeshes[0].m_triangleIndexStride = 3*sizeof(short int);
	}

	if (m_use4componentVertices)
	{
		m_indexedMeshes[0].m_numVertices = m_4componentVertices.size();
		m_indexedMeshes[0].m_vertexBase = (unsigned char*)&m_4componentVertices[0];
		m_indexedMeshes[0].m_vertexStride = sizeof(btVector3);
	} else
	{
		m_indexedMeshes[0].m_numVertices = m_3componentVertices.size()/3;
		m_indexedMeshes[0].m_vertexBase = (unsigned char*)&m_3componentVertices[0];
		m_indexedMeshes[0].m_vertexStride = 3*sizeof(btScalar);
	}


}

		
void	btTriangleMesh::addTriangle(const btVector3& vertex0,const btVector3& vertex1,const btVector3& vertex2)
{
	m_indexedMeshes[0].m_numTriangles++;
	m_indexedMeshes[0].m_numVertices+=3;

	if (m_use4componentVertices)
	{
		m_4componentVertices.push_back(vertex0);
		m_4componentVertices.push_back(vertex1);
		m_4componentVertices.push_back(vertex2);
		m_indexedMeshes[0].m_vertexBase = (unsigned char*)&m_4componentVertices[0];
	} else
	{
		m_3componentVertices.push_back(vertex0.getX());
		m_3componentVertices.push_back(vertex0.getY());
		m_3componentVertices.push_back(vertex0.getZ());

		m_3componentVertices.push_back(vertex1.getX());
		m_3componentVertices.push_back(vertex1.getY());
		m_3componentVertices.push_back(vertex1.getZ());

		m_3componentVertices.push_back(vertex2.getX());
		m_3componentVertices.push_back(vertex2.getY());
		m_3componentVertices.push_back(vertex2.getZ());
		m_indexedMeshes[0].m_vertexBase = (unsigned char*)&m_3componentVertices[0];
	}

	if (m_use32bitIndices)
	{
		int curIndex = m_32bitIndices.size();
		m_32bitIndices.push_back(curIndex++);
		m_32bitIndices.push_back(curIndex++);
		m_32bitIndices.push_back(curIndex++);
		m_indexedMeshes[0].m_triangleIndexBase = (unsigned char*) &m_32bitIndices[0];
	} else
	{
		short curIndex = static_cast<short>(m_16bitIndices.size());
		m_16bitIndices.push_back(curIndex++);
		m_16bitIndices.push_back(curIndex++);
		m_16bitIndices.push_back(curIndex++);
		m_indexedMeshes[0].m_triangleIndexBase = (unsigned char*) &m_16bitIndices[0];
	}
}

int btTriangleMesh::getNumTriangles() const
{
	if (m_use32bitIndices)
	{
		return m_32bitIndices.size() / 3;
	}
	return m_16bitIndices.size() / 3;
}
