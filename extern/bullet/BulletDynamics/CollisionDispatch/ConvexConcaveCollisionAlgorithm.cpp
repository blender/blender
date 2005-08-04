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
#include "ConvexConcaveCollisionAlgorithm.h"
#include "Dynamics/RigidBody.h"
#include "CollisionShapes/MultiSphereShape.h"
#include "ConstraintSolver/ContactConstraint.h"
#include "CollisionShapes/BoxShape.h"
#include "ConvexConvexAlgorithm.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include "CollisionShapes/TriangleShape.h"
#include "ConstraintSolver/ConstraintSolver.h"
#include "ConstraintSolver/ContactSolverInfo.h"
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
	  m_stepCount(-1)
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
 

	RigidBody* triangleBody = (RigidBody*)m_triangleProxy.m_clientObject;

	//aabb filter is already applied!	

	CollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = m_dispatcher;

	ConvexShape* tmp = static_cast<ConvexShape*>(triangleBody->GetCollisionShape());

	if (m_boxProxy->IsConvexShape())
	{
		TriangleShape tm(triangle[0],triangle[1],triangle[2]);	
		tm.SetMargin(m_collisionMarginTriangle);

		RigidBody* triangleBody = (RigidBody* )m_triangleProxy.m_clientObject;
		
		triangleBody->SetCollisionShape(&tm);
		ConvexConvexAlgorithm cvxcvxalgo(m_manifoldPtr,ci,m_boxProxy,&m_triangleProxy);
		triangleBody->SetCollisionShape(&tm);
		cvxcvxalgo.ProcessCollision(m_boxProxy,&m_triangleProxy,m_timeStep,m_stepCount,m_useContinuous);
	}

	triangleBody->SetCollisionShape(tmp);

}



void	BoxTriangleCallback::SetTimeStepAndCounters(float timeStep,int stepCount,float collisionMarginTriangle,bool useContinuous)
{
	m_timeStep = timeStep;
	m_stepCount = stepCount;
	m_useContinuous = useContinuous;
	m_collisionMarginTriangle = collisionMarginTriangle;

	//recalc aabbs
	RigidBody* boxBody = (RigidBody* )m_boxProxy->m_clientObject;
	RigidBody* triBody = (RigidBody* )m_triangleProxy.m_clientObject;

	SimdTransform boxInTriangleSpace;
	boxInTriangleSpace = triBody->getCenterOfMassTransform().inverse() * boxBody->getCenterOfMassTransform();

	boxBody->GetCollisionShape()->GetAabb(boxInTriangleSpace,m_aabbMin,m_aabbMax);

	float extraMargin = CONVEX_DISTANCE_MARGIN;//+0.1f;

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

		RigidBody* convexbody = (RigidBody* )m_convex.m_clientObject;
		RigidBody* concavebody = (RigidBody* )m_concave.m_clientObject;

			//todo: move this in the dispatcher
		if ((convexbody->GetActivationState() == 2) &&(concavebody->GetActivationState() == 2))
		return;


		TriangleMeshShape* triangleMesh = (TriangleMeshShape*) concavebody->GetCollisionShape();
		
		if (m_convex.IsConvexShape())
		{
			float collisionMarginTriangle = 0.02f;//triangleMesh->GetMargin();
					
			m_boxTriangleCallback.SetTimeStepAndCounters(timeStep,stepCount, collisionMarginTriangle,useContinuous);
#ifdef USE_BOX_TRIANGLE
			m_boxTriangleCallback.m_manifoldPtr->ClearManifold();
#endif
			m_boxTriangleCallback.m_manifoldPtr->SetBodies(convexbody,concavebody);		

			triangleMesh->ProcessAllTriangles( &m_boxTriangleCallback,m_boxTriangleCallback.GetAabbMin(),m_boxTriangleCallback.GetAabbMax());
			
	
		}

	}

}


float ConvexConcaveCollisionAlgorithm::CalculateTimeOfImpact(BroadphaseProxy* ,BroadphaseProxy* ,float timeStep,int stepCount)
{

	return 1.f;

	//quick approximation using raycast, todo: use proper continuou collision detection
	RigidBody* convexbody = (RigidBody* )m_convex.m_clientObject;
	const SimdVector3& from = convexbody->getCenterOfMassPosition();
		
	SimdVector3 radVec(0,0,0);
	
	float minradius = 0.05f;
	float lenSqr = convexbody->getLinearVelocity().length2();
	if (lenSqr > SIMD_EPSILON)
	{
		radVec = convexbody->getLinearVelocity();
		radVec.normalize();
		radVec *= minradius;
	}

	SimdVector3 to = from + radVec + convexbody->getLinearVelocity() * timeStep*1.01f;
	//only do if the motion exceeds the 'radius'


	RaycastCallback raycastCallback(from,to);

	raycastCallback.m_hitFraction = convexbody->m_hitFraction;

	SimdVector3 aabbMin (-1e30f,-1e30f,-1e30f);
	SimdVector3 aabbMax (SIMD_INFINITY,SIMD_INFINITY,SIMD_INFINITY);

	if (m_concave.GetClientObjectType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
	{

		RigidBody* concavebody = (RigidBody* )m_concave.m_clientObject;

		TriangleMeshShape* triangleMesh = (TriangleMeshShape*) concavebody->GetCollisionShape();
		
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
