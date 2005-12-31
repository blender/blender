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

//#define DISABLE_BVH


#include "CollisionShapes/BvhTriangleMeshShape.h"
#include "CollisionShapes/OptimizedBvh.h"

///Bvh Concave triangle mesh is a static-triangle mesh shape with Bounding Volume Hierarchy optimization.
///Uses an interface to access the triangles to allow for sharing graphics/physics triangles.
BvhTriangleMeshShape::BvhTriangleMeshShape(StridingMeshInterface* meshInterface)
:TriangleMeshShape(meshInterface)
{
	//construct bvh from meshInterface
#ifndef DISABLE_BVH

	m_bvh = new OptimizedBvh();
	m_bvh->Build(meshInterface);

#endif //DISABLE_BVH

}

BvhTriangleMeshShape::~BvhTriangleMeshShape()
{
	delete m_bvh;
}

//perform bvh tree traversal and report overlapping triangles to 'callback'
void	BvhTriangleMeshShape::ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{

#ifdef DISABLE_BVH
	//brute force traverse all triangles
	TriangleMeshShape::ProcessAllTriangles(callback,aabbMin,aabbMax);
#else

	//first get all the nodes

	
	struct	MyNodeOverlapCallback : public NodeOverlapCallback
	{
		StridingMeshInterface*	m_meshInterface;
		TriangleCallback*		m_callback;
		SimdVector3				m_triangle[3];


		MyNodeOverlapCallback(TriangleCallback* callback,StridingMeshInterface* meshInterface)
			:m_callback(callback),
			m_meshInterface(meshInterface)
		{
		}
				
		virtual void ProcessNode(const OptimizedBvhNode* node)
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
			
			const SimdVector3& meshScaling = m_meshInterface->getScaling();
			for (int j=2;j>=0;j--)
			{
				
				int graphicsindex = gfxbase[j];
#ifdef DEBUG_TRIANGLE_MESH
				printf("%d ,",graphicsindex);
#endif //DEBUG_TRIANGLE_MESH
				float* graphicsbase = (float*)(vertexbase+graphicsindex*stride);

				m_triangle[j] = SimdVector3(
					graphicsbase[0]*meshScaling.getX(),
					graphicsbase[1]*meshScaling.getY(),
					graphicsbase[2]*meshScaling.getZ());
#ifdef DEBUG_TRIANGLE_MESH
				printf("triangle vertices:%f,%f,%f\n",triangle[j].x(),triangle[j].y(),triangle[j].z());
#endif //DEBUG_TRIANGLE_MESH
			}

			m_callback->ProcessTriangle(m_triangle);
			m_meshInterface->unLockReadOnlyVertexBase(node->m_subPart);
		}

	};

	MyNodeOverlapCallback	myNodeCallback(callback,m_meshInterface);

	m_bvh->ReportAabbOverlappingNodex(&myNodeCallback,aabbMin,aabbMax);


#endif//DISABLE_BVH


}


void	BvhTriangleMeshShape::setLocalScaling(const SimdVector3& scaling)
{
	if ((getLocalScaling() -scaling).length2() > SIMD_EPSILON)
	{
		TriangleMeshShape::setLocalScaling(scaling);
		delete m_bvh;
		m_bvh = new OptimizedBvh();
		m_bvh->Build(m_meshInterface);
		//rebuild the bvh...
	}
}
