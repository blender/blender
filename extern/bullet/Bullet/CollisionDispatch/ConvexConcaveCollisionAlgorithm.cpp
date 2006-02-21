/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#include "ConvexConcaveCollisionAlgorithm.h"
#include "NarrowPhaseCollision/CollisionObject.h"
#include "CollisionShapes/MultiSphereShape.h"
#include "CollisionShapes/BoxShape.h"
#include "ConvexConvexAlgorithm.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include "CollisionShapes/TriangleShape.h"
#include "CollisionDispatch/ManifoldResult.h"
#include "NarrowPhaseCollision/RaycastCallback.h"
#include "CollisionShapes/TriangleMeshShape.h"


ConvexConcaveCollisionAlgorithm::ConvexConcaveCollisionAlgorithm( const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
: CollisionAlgorithm(ci),m_convex(*proxy0),m_concave(*proxy1),
m_boxTriangleCallback(ci.m_dispatcher,proxy0,proxy1)
{
}

ConvexConcaveCollisionAlgorithm::~ConvexConcaveCollisionAlgorithm()
{
}



BoxTriangleCallback::BoxTriangleCallback(Dispatcher*  dispatcher,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1):
  m_boxProxy(proxy0),m_triangleProxy(*proxy1),m_dispatcher(dispatcher),
	  m_timeStep(0.f),
	  m_stepCount(-1),
	  m_triangleCount(0)
{

	  m_triangleProxy.SetClientObjectType(TRIANGLE_SHAPE_PROXYTYPE);

	  //
	  // create the manifold from the dispatcher 'manifold pool'
	  //
	  m_manifoldPtr = m_dispatcher->GetNewManifold(proxy0->m_clientObject,proxy1->m_clientObject);

  	  ClearCache();
}

BoxTriangleCallback::~BoxTriangleCallback()
{
	ClearCache();
	m_dispatcher->ReleaseManifold( m_manifoldPtr );
  
}
  

void	BoxTriangleCallback::ClearCache()
{

	m_manifoldPtr->ClearManifold();
};



void BoxTriangleCallback::ProcessTriangle(SimdVector3* triangle)
{
 
	//just for debugging purposes
	//printf("triangle %d",m_triangleCount++);


	//aabb filter is already applied!	

	CollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = m_dispatcher;

	

	if (m_boxProxy->IsConvexShape())
	{
		TriangleShape tm(triangle[0],triangle[1],triangle[2]);	
		tm.SetMargin(m_collisionMarginTriangle);
	
		CollisionObject* ob = static_cast<CollisionObject*>(m_triangleProxy.m_clientObject);

		CollisionShape* tmpShape = ob->m_collisionShape;
		ob->m_collisionShape = &tm;
		
		ConvexConvexAlgorithm cvxcvxalgo(m_manifoldPtr,ci,m_boxProxy,&m_triangleProxy);
		cvxcvxalgo.ProcessCollision(m_boxProxy,&m_triangleProxy,m_timeStep,m_stepCount,m_useContinuous);
		ob->m_collisionShape = tmpShape;

	}

	

}



void	BoxTriangleCallback::SetTimeStepAndCounters(float timeStep,int stepCount,float collisionMarginTriangle,bool useContinuous)
{
	m_triangleCount = 0;
	m_timeStep = timeStep;
	m_stepCount = stepCount;
	m_useContinuous = useContinuous;
	m_collisionMarginTriangle = collisionMarginTriangle;

	//recalc aabbs
	CollisionObject* boxBody = (CollisionObject* )m_boxProxy->m_clientObject;
	CollisionObject* triBody = (CollisionObject* )m_triangleProxy.m_clientObject;

	SimdTransform boxInTriangleSpace;
	boxInTriangleSpace = triBody->m_worldTransform.inverse() * boxBody->m_worldTransform;

	CollisionShape* boxshape = static_cast<CollisionShape*>(boxBody->m_collisionShape);
	CollisionShape* triangleShape = static_cast<CollisionShape*>(triBody->m_collisionShape);

	boxshape->GetAabb(boxInTriangleSpace,m_aabbMin,m_aabbMax);

	float extraMargin = collisionMarginTriangle;//CONVEX_DISTANCE_MARGIN;//+0.1f;

	SimdVector3 extra(extraMargin,extraMargin,extraMargin);

	m_aabbMax += extra;
	m_aabbMin -= extra;
	
}

void ConvexConcaveCollisionAlgorithm::ClearCache()
{
	m_boxTriangleCallback.ClearCache();

}

void ConvexConcaveCollisionAlgorithm::ProcessCollision (BroadphaseProxy* ,BroadphaseProxy* ,float timeStep,int stepCount,bool useContinuous)
{

	if (m_concave.GetClientObjectType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
	{

		if (!m_dispatcher->NeedsCollision(m_convex,m_concave))
			return;

		

		CollisionObject*	triOb = static_cast<CollisionObject*>(m_concave.m_clientObject);
		TriangleMeshShape* triangleMesh = static_cast<TriangleMeshShape*>( triOb->m_collisionShape);
		
		if (m_convex.IsConvexShape())
		{
			float collisionMarginTriangle = triangleMesh->GetMargin();
					
			m_boxTriangleCallback.SetTimeStepAndCounters(timeStep,stepCount, collisionMarginTriangle,useContinuous);
#ifdef USE_BOX_TRIANGLE
			m_boxTriangleCallback.m_manifoldPtr->ClearManifold();
#endif
			m_boxTriangleCallback.m_manifoldPtr->SetBodies(m_convex.m_clientObject,m_concave.m_clientObject);

			triangleMesh->ProcessAllTriangles( &m_boxTriangleCallback,m_boxTriangleCallback.GetAabbMin(),m_boxTriangleCallback.GetAabbMax());
			
	
		}

	}

}


float ConvexConcaveCollisionAlgorithm::CalculateTimeOfImpact(BroadphaseProxy* ,BroadphaseProxy* ,float timeStep,int stepCount)
{

	//quick approximation using raycast, todo: use proper continuou collision detection
	CollisionObject* convexbody = (CollisionObject* )m_convex.m_clientObject;
	const SimdVector3& from = convexbody->m_worldTransform.getOrigin();
	
	SimdVector3 to = convexbody->m_nextPredictedWorldTransform.getOrigin();
	//only do if the motion exceeds the 'radius'


	RaycastCallback raycastCallback(from,to);

	raycastCallback.m_hitFraction = convexbody->m_hitFraction;

	SimdVector3 aabbMin (-1e30f,-1e30f,-1e30f);
	SimdVector3 aabbMax (SIMD_INFINITY,SIMD_INFINITY,SIMD_INFINITY);

	if (m_concave.GetClientObjectType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
	{

		CollisionObject* concavebody = (CollisionObject* )m_concave.m_clientObject;

		TriangleMeshShape* triangleMesh = (TriangleMeshShape*) concavebody->m_collisionShape;
		
		if (triangleMesh)
		{
			triangleMesh->ProcessAllTriangles(&raycastCallback,aabbMin,aabbMax);
		}
	}


	if (raycastCallback.m_hitFraction < convexbody->m_hitFraction)
	{
		convexbody->m_hitFraction = raycastCallback.m_hitFraction;
		return raycastCallback.m_hitFraction;
	}

	return 1.f;

}
