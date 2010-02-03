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
btBvhTriangleMeshShape::btBvhTriangleMeshShape(btStridingMeshInterface* meshInterface, bool useQuantizedAabbCompression, bool buildBvh)
:btTriangleMeshShape(meshInterface),
m_bvh(0),
m_useQuantizedAabbCompression(useQuantizedAabbCompression),
m_ownsBvh(false)
{
	m_shapeType = TRIANGLE_MESH_SHAPE_PROXYTYPE;
	//construct bvh from meshInterface
#ifndef DISABLE_BVH

	if (buildBvh)
	{
		buildOptimizedBvh();
	}

#endif //DISABLE_BVH

}

btBvhTriangleMeshShape::btBvhTriangleMeshShape(btStridingMeshInterface* meshInterface, bool useQuantizedAabbCompression,const btVector3& bvhAabbMin,const btVector3& bvhAabbMax,bool buildBvh)
:btTriangleMeshShape(meshInterface),
m_bvh(0),
m_useQuantizedAabbCompression(useQuantizedAabbCompression),
m_ownsBvh(false)
{
	m_shapeType = TRIANGLE_MESH_SHAPE_PROXYTYPE;
	//construct bvh from meshInterface
#ifndef DISABLE_BVH

	if (buildBvh)
	{
		void* mem = btAlignedAlloc(sizeof(btOptimizedBvh),16);
		m_bvh = new (mem) btOptimizedBvh();
		
		m_bvh->build(meshInterface,m_useQuantizedAabbCompression,bvhAabbMin,bvhAabbMax);
		m_ownsBvh = true;
	}

#endif //DISABLE_BVH

}

void	btBvhTriangleMeshShape::partialRefitTree(const btVector3& aabbMin,const btVector3& aabbMax)
{
	m_bvh->refitPartial( m_meshInterface,aabbMin,aabbMax );
	
	m_localAabbMin.setMin(aabbMin);
	m_localAabbMax.setMax(aabbMax);
}


void	btBvhTriangleMeshShape::refitTree(const btVector3& aabbMin,const btVector3& aabbMax)
{
	m_bvh->refit( m_meshInterface, aabbMin,aabbMax );
	
	recalcLocalAabb();
}

btBvhTriangleMeshShape::~btBvhTriangleMeshShape()
{
	if (m_ownsBvh)
	{
		m_bvh->~btOptimizedBvh();
		btAlignedFree(m_bvh);
	}
}

void	btBvhTriangleMeshShape::performRaycast (btTriangleCallback* callback, const btVector3& raySource, const btVector3& rayTarget)
{
	struct	MyNodeOverlapCallback : public btNodeOverlapCallback
	{
		btStridingMeshInterface*	m_meshInterface;
		btTriangleCallback* m_callback;

		MyNodeOverlapCallback(btTriangleCallback* callback,btStridingMeshInterface* meshInterface)
			:m_meshInterface(meshInterface),
			m_callback(callback)
		{
		}
				
		virtual void processNode(int nodeSubPart, int nodeTriangleIndex)
		{
			btVector3 m_triangle[3];
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
				nodeSubPart);

			unsigned int* gfxbase = (unsigned int*)(indexbase+nodeTriangleIndex*indexstride);
			btAssert(indicestype==PHY_INTEGER||indicestype==PHY_SHORT);
	
			const btVector3& meshScaling = m_meshInterface->getScaling();
			for (int j=2;j>=0;j--)
			{
				int graphicsindex = indicestype==PHY_SHORT?((unsigned short*)gfxbase)[j]:gfxbase[j];
				
				if (type == PHY_FLOAT)
				{
					float* graphicsbase = (float*)(vertexbase+graphicsindex*stride);
					
					m_triangle[j] = btVector3(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),graphicsbase[2]*meshScaling.getZ());		
				}
				else
				{
					double* graphicsbase = (double*)(vertexbase+graphicsindex*stride);
					
					m_triangle[j] = btVector3(btScalar(graphicsbase[0])*meshScaling.getX(),btScalar(graphicsbase[1])*meshScaling.getY(),btScalar(graphicsbase[2])*meshScaling.getZ());		
				}
			}

			/* Perform ray vs. triangle collision here */
			m_callback->processTriangle(m_triangle,nodeSubPart,nodeTriangleIndex);
			m_meshInterface->unLockReadOnlyVertexBase(nodeSubPart);
		}
	};

	MyNodeOverlapCallback	myNodeCallback(callback,m_meshInterface);

	m_bvh->reportRayOverlappingNodex(&myNodeCallback,raySource,rayTarget);
}

void	btBvhTriangleMeshShape::performConvexcast (btTriangleCallback* callback, const btVector3& raySource, const btVector3& rayTarget, const btVector3& aabbMin, const btVector3& aabbMax)
{
	struct	MyNodeOverlapCallback : public btNodeOverlapCallback
	{
		btStridingMeshInterface*	m_meshInterface;
		btTriangleCallback* m_callback;

		MyNodeOverlapCallback(btTriangleCallback* callback,btStridingMeshInterface* meshInterface)
			:m_meshInterface(meshInterface),
			m_callback(callback)
		{
		}
				
		virtual void processNode(int nodeSubPart, int nodeTriangleIndex)
		{
			btVector3 m_triangle[3];
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
				nodeSubPart);

			unsigned int* gfxbase = (unsigned int*)(indexbase+nodeTriangleIndex*indexstride);
			btAssert(indicestype==PHY_INTEGER||indicestype==PHY_SHORT);
	
			const btVector3& meshScaling = m_meshInterface->getScaling();
			for (int j=2;j>=0;j--)
			{
				int graphicsindex = indicestype==PHY_SHORT?((unsigned short*)gfxbase)[j]:gfxbase[j];

				if (type == PHY_FLOAT)
				{
					float* graphicsbase = (float*)(vertexbase+graphicsindex*stride);

					m_triangle[j] = btVector3(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),graphicsbase[2]*meshScaling.getZ());		
				}
				else
				{
					double* graphicsbase = (double*)(vertexbase+graphicsindex*stride);
					
					m_triangle[j] = btVector3(btScalar(graphicsbase[0])*meshScaling.getX(),btScalar(graphicsbase[1])*meshScaling.getY(),btScalar(graphicsbase[2])*meshScaling.getZ());		
				}
			}

			/* Perform ray vs. triangle collision here */
			m_callback->processTriangle(m_triangle,nodeSubPart,nodeTriangleIndex);
			m_meshInterface->unLockReadOnlyVertexBase(nodeSubPart);
		}
	};

	MyNodeOverlapCallback	myNodeCallback(callback,m_meshInterface);

	m_bvh->reportBoxCastOverlappingNodex (&myNodeCallback, raySource, rayTarget, aabbMin, aabbMax);
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
				
		virtual void processNode(int nodeSubPart, int nodeTriangleIndex)
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
				nodeSubPart);

			unsigned int* gfxbase = (unsigned int*)(indexbase+nodeTriangleIndex*indexstride);
			btAssert(indicestype==PHY_INTEGER||indicestype==PHY_SHORT);
	
			const btVector3& meshScaling = m_meshInterface->getScaling();
			for (int j=2;j>=0;j--)
			{
				
				int graphicsindex = indicestype==PHY_SHORT?((unsigned short*)gfxbase)[j]:gfxbase[j];


#ifdef DEBUG_TRIANGLE_MESH
				printf("%d ,",graphicsindex);
#endif //DEBUG_TRIANGLE_MESH
				if (type == PHY_FLOAT)
				{
					float* graphicsbase = (float*)(vertexbase+graphicsindex*stride);
					
					m_triangle[j] = btVector3(
																		graphicsbase[0]*meshScaling.getX(),
																		graphicsbase[1]*meshScaling.getY(),
																		graphicsbase[2]*meshScaling.getZ());
				}
				else
				{
					double* graphicsbase = (double*)(vertexbase+graphicsindex*stride);

					m_triangle[j] = btVector3(
						btScalar(graphicsbase[0])*meshScaling.getX(),
						btScalar(graphicsbase[1])*meshScaling.getY(),
						btScalar(graphicsbase[2])*meshScaling.getZ());
				}
#ifdef DEBUG_TRIANGLE_MESH
				printf("triangle vertices:%f,%f,%f\n",triangle[j].x(),triangle[j].y(),triangle[j].z());
#endif //DEBUG_TRIANGLE_MESH
			}

			m_callback->processTriangle(m_triangle,nodeSubPart,nodeTriangleIndex);
			m_meshInterface->unLockReadOnlyVertexBase(nodeSubPart);
		}

	};

	MyNodeOverlapCallback	myNodeCallback(callback,m_meshInterface);

	m_bvh->reportAabbOverlappingNodex(&myNodeCallback,aabbMin,aabbMax);


#endif//DISABLE_BVH


}

void   btBvhTriangleMeshShape::setLocalScaling(const btVector3& scaling)
{
   if ((getLocalScaling() -scaling).length2() > SIMD_EPSILON)
   {
      btTriangleMeshShape::setLocalScaling(scaling);
	  buildOptimizedBvh();
   }
}

void   btBvhTriangleMeshShape::buildOptimizedBvh()
{
	if (m_ownsBvh)
	{
		m_bvh->~btOptimizedBvh();
		btAlignedFree(m_bvh);
	}
	///m_localAabbMin/m_localAabbMax is already re-calculated in btTriangleMeshShape. We could just scale aabb, but this needs some more work
	void* mem = btAlignedAlloc(sizeof(btOptimizedBvh),16);
	m_bvh = new(mem) btOptimizedBvh();
	//rebuild the bvh...
	m_bvh->build(m_meshInterface,m_useQuantizedAabbCompression,m_localAabbMin,m_localAabbMax);
	m_ownsBvh = true;
}

void   btBvhTriangleMeshShape::setOptimizedBvh(btOptimizedBvh* bvh, const btVector3& scaling)
{
   btAssert(!m_bvh);
   btAssert(!m_ownsBvh);

   m_bvh = bvh;
   m_ownsBvh = false;
   // update the scaling without rebuilding the bvh
   if ((getLocalScaling() -scaling).length2() > SIMD_EPSILON)
   {
      btTriangleMeshShape::setLocalScaling(scaling);
   }
}


