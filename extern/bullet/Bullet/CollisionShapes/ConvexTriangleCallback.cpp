#include "ConvexTriangleCallback.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "NarrowPhaseCollision/ManifoldContactAddResult.h"
#include "NarrowPhaseCollision/GjkPairDetector.h"
#include "NarrowPhaseCollision/MinkowskiPenetrationDepthSolver.h"



#include "TriangleShape.h"

//m_manifoldPtr = m_dispatcher->GetNewManifold(proxy0->m_clientObject,proxy1->m_clientObject);
	//m_dispatcher->ReleaseManifold( m_manifoldPtr );

ConvexTriangleCallback::ConvexTriangleCallback(PersistentManifold* manifold,ConvexShape* convexShape,const SimdTransform&convexTransform,const SimdTransform& triangleMeshTransform)
:m_triangleMeshTransform(triangleMeshTransform),
	m_convexTransform(convexTransform),
	m_convexShape(convexShape),
	m_manifoldPtr(manifold),
	m_triangleCount(0)
{
}

ConvexTriangleCallback::~ConvexTriangleCallback()
{
  
}
  

void	ConvexTriangleCallback::ClearCache()
{
	m_manifoldPtr->ClearManifold();
};



void ConvexTriangleCallback::ProcessTriangle(SimdVector3* triangle)
{

	//triangle, convex
	TriangleShape tm(triangle[0],triangle[1],triangle[2]);	
	tm.SetMargin(m_collisionMarginTriangle);
	GjkPairDetector::ClosestPointInput input;
	input.m_transformA = m_triangleMeshTransform;
	input.m_transformB = m_convexTransform;
	
	ManifoldContactAddResult output(m_triangleMeshTransform,m_convexTransform,m_manifoldPtr);
	
	
	VoronoiSimplexSolver simplexSolver;
	MinkowskiPenetrationDepthSolver	penetrationDepthSolver;
	
	GjkPairDetector gjkDetector(&tm,m_convexShape,&simplexSolver,&penetrationDepthSolver);

	gjkDetector.SetMinkowskiA(&tm);
	gjkDetector.SetMinkowskiB(m_convexShape);
	input.m_maximumDistanceSquared = tm.GetMargin()+ m_convexShape->GetMargin() + m_manifoldPtr->GetManifoldMargin();
	input.m_maximumDistanceSquared*= input.m_maximumDistanceSquared;

	input.m_maximumDistanceSquared = 1e30f;//?
	
		
	gjkDetector.GetClosestPoints(input,output);


}



void	ConvexTriangleCallback::Update(float collisionMarginTriangle)
{
	m_triangleCount = 0;
	m_collisionMarginTriangle = collisionMarginTriangle;

	SimdTransform boxInTriangleSpace;
	boxInTriangleSpace = m_triangleMeshTransform.inverse() * m_convexTransform;

	m_convexShape->GetAabb(boxInTriangleSpace,m_aabbMin,m_aabbMax);

	float extraMargin = CONVEX_DISTANCE_MARGIN;

	SimdVector3 extra(extraMargin,extraMargin,extraMargin);

	m_aabbMax += extra;
	m_aabbMin -= extra;
	
}
