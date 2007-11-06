
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

#include "btDefaultCollisionConfiguration.h"

#include "BulletCollision/CollisionDispatch/btConvexConvexAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btEmptyCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btConvexConcaveCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btCompoundCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btSphereSphereCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btSphereBoxCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btSphereTriangleCollisionAlgorithm.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"



#include "LinearMath/btStackAlloc.h"
#include "LinearMath/btPoolAllocator.h"



#define DEFAULT_MAX_OVERLAPPING_PAIRS 65535
#define DEFAULT_STACK_ALLOCATOR_SIZE	(5*1024*1024)


btDefaultCollisionConfiguration::btDefaultCollisionConfiguration(btStackAlloc*	stackAlloc,btPoolAllocator*	persistentManifoldPool,btPoolAllocator*	collisionAlgorithmPool)
{

	void* mem = btAlignedAlloc(sizeof(btVoronoiSimplexSolver),16);
	m_simplexSolver = new (mem)btVoronoiSimplexSolver();
	mem = btAlignedAlloc(sizeof(btGjkEpaPenetrationDepthSolver),16);
	m_pdSolver = new (mem)btGjkEpaPenetrationDepthSolver;

	//default CreationFunctions, filling the m_doubleDispatch table
	mem = btAlignedAlloc(sizeof(btConvexConvexAlgorithm::CreateFunc),16);
	m_convexConvexCreateFunc = new(mem) btConvexConvexAlgorithm::CreateFunc(m_simplexSolver,m_pdSolver);
	mem = btAlignedAlloc(sizeof(btConvexConcaveCollisionAlgorithm::CreateFunc),16);
	m_convexConcaveCreateFunc = new (mem)btConvexConcaveCollisionAlgorithm::CreateFunc;
	mem = btAlignedAlloc(sizeof(btConvexConcaveCollisionAlgorithm::CreateFunc),16);
	m_swappedConvexConcaveCreateFunc = new (mem)btConvexConcaveCollisionAlgorithm::SwappedCreateFunc;
	mem = btAlignedAlloc(sizeof(btCompoundCollisionAlgorithm::CreateFunc),16);
	m_compoundCreateFunc = new (mem)btCompoundCollisionAlgorithm::CreateFunc;
	mem = btAlignedAlloc(sizeof(btCompoundCollisionAlgorithm::SwappedCreateFunc),16);
	m_swappedCompoundCreateFunc = new (mem)btCompoundCollisionAlgorithm::SwappedCreateFunc;
	mem = btAlignedAlloc(sizeof(btEmptyAlgorithm::CreateFunc),16);
	m_emptyCreateFunc = new(mem) btEmptyAlgorithm::CreateFunc;

	mem = btAlignedAlloc(sizeof(btSphereSphereCollisionAlgorithm::CreateFunc),16);
	m_sphereSphereCF = new(mem) btSphereSphereCollisionAlgorithm::CreateFunc;
	mem = btAlignedAlloc(sizeof(btSphereBoxCollisionAlgorithm::CreateFunc),16);
	m_sphereBoxCF = new(mem) btSphereBoxCollisionAlgorithm::CreateFunc;
	mem = btAlignedAlloc(sizeof(btSphereBoxCollisionAlgorithm::CreateFunc),16);
	m_boxSphereCF = new (mem)btSphereBoxCollisionAlgorithm::CreateFunc;
	m_boxSphereCF->m_swapped = true;
	mem = btAlignedAlloc(sizeof(btSphereTriangleCollisionAlgorithm::CreateFunc),16);
	m_sphereTriangleCF = new (mem)btSphereTriangleCollisionAlgorithm::CreateFunc;
	mem = btAlignedAlloc(sizeof(btSphereTriangleCollisionAlgorithm::CreateFunc),16);
	m_triangleSphereCF = new (mem)btSphereTriangleCollisionAlgorithm::CreateFunc;
	m_triangleSphereCF->m_swapped = true;


	///calculate maximum element size, big enough to fit any collision algorithm in the memory pool
	int maxSize = sizeof(btConvexConvexAlgorithm);
	int maxSize2 = sizeof(btConvexConcaveCollisionAlgorithm);
	int maxSize3 = sizeof(btCompoundCollisionAlgorithm);
	int maxSize4 = sizeof(btEmptyAlgorithm);
	
	int	collisionAlgorithmMaxElementSize = btMax(maxSize,maxSize2);
	collisionAlgorithmMaxElementSize = btMax(collisionAlgorithmMaxElementSize,maxSize3);
	collisionAlgorithmMaxElementSize = btMax(collisionAlgorithmMaxElementSize,maxSize4);

	if (stackAlloc)
	{
		m_ownsStackAllocator = false;
		this->m_stackAlloc = stackAlloc;
	} else
	{
		m_ownsStackAllocator = true;
		void* mem = btAlignedAlloc(sizeof(btStackAlloc),16);
		m_stackAlloc = new(mem)btStackAlloc(DEFAULT_STACK_ALLOCATOR_SIZE);
	}
		
	if (persistentManifoldPool)
	{
		m_ownsPersistentManifoldPool = false;
		m_persistentManifoldPool = persistentManifoldPool;
	} else
	{
		m_ownsPersistentManifoldPool = true;
		void* mem = btAlignedAlloc(sizeof(btPoolAllocator),16);
		m_persistentManifoldPool = new (mem) btPoolAllocator(sizeof(btPersistentManifold),DEFAULT_MAX_OVERLAPPING_PAIRS);
	}
	
	if (collisionAlgorithmPool)
	{
		m_ownsCollisionAlgorithmPool = false;
		m_collisionAlgorithmPool = collisionAlgorithmPool;
	} else
	{
		m_ownsCollisionAlgorithmPool = true;
		void* mem = btAlignedAlloc(sizeof(btPoolAllocator),16);
		m_collisionAlgorithmPool = new(mem) btPoolAllocator(collisionAlgorithmMaxElementSize,DEFAULT_MAX_OVERLAPPING_PAIRS);
	}


}

btDefaultCollisionConfiguration::~btDefaultCollisionConfiguration()
{
	if (m_ownsStackAllocator)
	{
		m_stackAlloc->destroy();
		m_stackAlloc->~btStackAlloc();
		btAlignedFree(m_stackAlloc);
	}
	if (m_ownsCollisionAlgorithmPool)
	{
		m_collisionAlgorithmPool->~btPoolAllocator();
		btAlignedFree(m_collisionAlgorithmPool);
	}
	if (m_ownsPersistentManifoldPool)
	{
		m_persistentManifoldPool->~btPoolAllocator();
		btAlignedFree(m_persistentManifoldPool);
	}

	m_convexConvexCreateFunc->~btCollisionAlgorithmCreateFunc();
	btAlignedFree(	m_convexConvexCreateFunc);

	m_convexConcaveCreateFunc->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_convexConcaveCreateFunc);
	m_swappedConvexConcaveCreateFunc->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_swappedConvexConcaveCreateFunc);

	m_compoundCreateFunc->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_compoundCreateFunc);

	m_swappedCompoundCreateFunc->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_swappedCompoundCreateFunc);

	m_emptyCreateFunc->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_emptyCreateFunc);

	m_sphereSphereCF->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_sphereSphereCF);

	m_sphereBoxCF->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_sphereBoxCF);
	m_boxSphereCF->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_boxSphereCF);
	m_sphereTriangleCF->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_sphereTriangleCF);
	m_triangleSphereCF->~btCollisionAlgorithmCreateFunc();
	btAlignedFree( m_triangleSphereCF);

	m_simplexSolver->~btVoronoiSimplexSolver();
	btAlignedFree(m_simplexSolver);
	m_pdSolver->~btGjkEpaPenetrationDepthSolver();
	btAlignedFree(m_pdSolver);


}


btCollisionAlgorithmCreateFunc* btDefaultCollisionConfiguration::getCollisionAlgorithmCreateFunc(int proxyType0,int proxyType1)
{
	

	if ((proxyType0 == SPHERE_SHAPE_PROXYTYPE) && (proxyType1==SPHERE_SHAPE_PROXYTYPE))
	{
		return	m_sphereSphereCF;
	}

	if ((proxyType0 == SPHERE_SHAPE_PROXYTYPE) && (proxyType1==BOX_SHAPE_PROXYTYPE))
	{
		return	m_sphereBoxCF;
	}

	if ((proxyType0 == BOX_SHAPE_PROXYTYPE ) && (proxyType1==SPHERE_SHAPE_PROXYTYPE))
	{
		return	m_boxSphereCF;
	}

	if ((proxyType0 == SPHERE_SHAPE_PROXYTYPE ) && (proxyType1==TRIANGLE_SHAPE_PROXYTYPE))
	{
		return	m_sphereTriangleCF;
	}

	if ((proxyType0 == TRIANGLE_SHAPE_PROXYTYPE  ) && (proxyType1==SPHERE_SHAPE_PROXYTYPE))
	{
		return	m_triangleSphereCF;
	}
	

	if (btBroadphaseProxy::isConvex(proxyType0) && btBroadphaseProxy::isConvex(proxyType1))
	{
		return m_convexConvexCreateFunc;
	}

	if (btBroadphaseProxy::isConvex(proxyType0) && btBroadphaseProxy::isConcave(proxyType1))
	{
		return m_convexConcaveCreateFunc;
	}

	if (btBroadphaseProxy::isConvex(proxyType1) && btBroadphaseProxy::isConcave(proxyType0))
	{
		return m_swappedConvexConcaveCreateFunc;
	}

	if (btBroadphaseProxy::isCompound(proxyType0))
	{
		return m_compoundCreateFunc;
	} else
	{
		if (btBroadphaseProxy::isCompound(proxyType1))
		{
			return m_swappedCompoundCreateFunc;
		}
	}

	//failed to find an algorithm
	return m_emptyCreateFunc;
}
