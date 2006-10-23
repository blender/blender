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

//#define DISABLE_BVH


#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"
#include "BulletCollision/CollisionShapes/btOptimizedBvh.h"

///Bvh Concave triangle mesh is a static-triangle mesh shape with Bounding Volume Hierarchy optimization.
///Uses an interface to access the triangles to allow for sharing graphics/physics triangles.
btBvhTriangleMeshShape::btBvhTriangleMeshShape(btStridingMeshInterface* meshInterface)
:btTriangleMeshShape(meshInterface)
{
	//construct bvh from meshInterface
#ifndef DISABLE_BVH

	m_bvh = new btOptimizedBvh();
	m_bvh->build(meshInterface);

#endif //DISABLE_BVH

}

btBvhTriangleMeshShape::~btBvhTriangleMeshShape()
{
	delete m_bvh;
}

//perform bvh tree traversal and report overlapping triangles to 'callback'
void	btBvhTriangleMeshShape::processAllTriangles(btTriangleCallback* callback,const btVector3& aabbMin,const btVector3& aabbMax) const
{

#ifdef DISABLE_BVH
	//brute force traverse all triangles
	btTriangleMeshShape::processAllTriangles(callback,aabbMin,aabbMax);
#else

	//first get all the nodes

	
	struct	MyNodeOverlapCallback : public btNodeOverlapCallback
	{
		btStridingMeshInterface*	m_meshInterface;
		btTriangleCallback*		m_callback;
		btVector3				m_triangle[3];


		MyNodeOverlapCallback(btTriangleCallback* callback,btStridingMeshInterface* meshInterface)
			:m_meshInterface(meshInterface),
			m_callback(callback)
		{
		}
				
		virtual void processNode(const btOptimizedBvhNode* node)
		{
			const unsigned char *vertexbase;
			int numverts;
			PHY_ScalarType type;
			int stride;
			const unsigned char *indexbase;
			int indexstride;
			int numfaces;
			PHY_ScalarType indicestype;
			

			m_meshInterface->getLockedReadOnlyVertexIndexBase(
				&vertexbase,
				numverts,
				type,
				stride,
				&indexbase,
				indexstride,
				numfaces,
				indicestype,
				node->m_subPart);

			int* gfxbase = (int*)(indexbase+node->m_triangleIndex*indexstride);
			
			const btVector3& meshScaling = m_meshInterface->getScaling();
			for (int j=2;j>=0;j--)
			{
				
				int graphicsindex = gfxbase[j];
#ifdef DEBUG_TRIANGLE_MESH
				printf("%d ,",graphicsindex);
#endif //DEBUG_TRIANGLE_MESH
				float* graphicsbase = (float*)(vertexbase+graphicsindex*stride);

				m_triangle[j] = btVector3(
					graphicsbase[0]*meshScaling.getX(),
					graphicsbase[1]*meshScaling.getY(),
					graphicsbase[2]*meshScaling.getZ());
#ifdef DEBUG_TRIANGLE_MESH
				printf("triangle vertices:%f,%f,%f\n",triangle[j].x(),triangle[j].y(),triangle[j].z());
#endif //DEBUG_TRIANGLE_MESH
			}

			m_callback->processTriangle(m_triangle,node->m_subPart,node->m_triangleIndex);
			m_meshInterface->unLockReadOnlyVertexBase(node->m_subPart);
		}

	};

	MyNodeOverlapCallback	myNodeCallback(callback,m_meshInterface);

	m_bvh->reportAabbOverlappingNodex(&myNodeCallback,aabbMin,aabbMax);


#endif//DISABLE_BVH


}


void	btBvhTriangleMeshShape::setLocalScaling(const btVector3& scaling)
{
	if ((getLocalScaling() -scaling).length2() > SIMD_EPSILON)
	{
		btTriangleMeshShape::setLocalScaling(scaling);
		delete m_bvh;
		m_bvh = new btOptimizedBvh();
		m_bvh->build(m_meshInterface);
		//rebuild the bvh...
	}
}
