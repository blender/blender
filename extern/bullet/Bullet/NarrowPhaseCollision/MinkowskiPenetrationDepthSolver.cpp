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

#include "MinkowskiPenetrationDepthSolver.h"
#include "CollisionShapes/MinkowskiSumShape.h"
#include "NarrowPhaseCollision/SubSimplexConvexCast.h"
#include "NarrowPhaseCollision/VoronoiSimplexSolver.h"
#include "NarrowPhaseCollision/GjkPairDetector.h"


struct MyResult : public DiscreteCollisionDetectorInterface::Result
{

	MyResult():m_hasResult(false)
	{
	}
	
	SimdVector3 m_normalOnBInWorld;
	SimdVector3 m_pointInWorld;
	float m_depth;
	bool	m_hasResult;

	void AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)
	{
		m_normalOnBInWorld = normalOnBInWorld;
		m_pointInWorld = pointInWorld;
		m_depth = depth;
		m_hasResult = true;
	}
};

#define NUM_UNITSPHERE_POINTS 42
static SimdVector3	sPenetrationDirections[NUM_UNITSPHERE_POINTS] = 
{
SimdVector3(0.000000f , -0.000000f,-1.000000f),
SimdVector3(0.723608f , -0.525725f,-0.447219f),
SimdVector3(-0.276388f , -0.850649f,-0.447219f),
SimdVector3(-0.894426f , -0.000000f,-0.447216f),
SimdVector3(-0.276388f , 0.850649f,-0.447220f),
SimdVector3(0.723608f , 0.525725f,-0.447219f),
SimdVector3(0.276388f , -0.850649f,0.447220f),
SimdVector3(-0.723608f , -0.525725f,0.447219f),
SimdVector3(-0.723608f , 0.525725f,0.447219f),
SimdVector3(0.276388f , 0.850649f,0.447219f),
SimdVector3(0.894426f , 0.000000f,0.447216f),
SimdVector3(-0.000000f , 0.000000f,1.000000f),
SimdVector3(0.425323f , -0.309011f,-0.850654f),
SimdVector3(-0.162456f , -0.499995f,-0.850654f),
SimdVector3(0.262869f , -0.809012f,-0.525738f),
SimdVector3(0.425323f , 0.309011f,-0.850654f),
SimdVector3(0.850648f , -0.000000f,-0.525736f),
SimdVector3(-0.525730f , -0.000000f,-0.850652f),
SimdVector3(-0.688190f , -0.499997f,-0.525736f),
SimdVector3(-0.162456f , 0.499995f,-0.850654f),
SimdVector3(-0.688190f , 0.499997f,-0.525736f),
SimdVector3(0.262869f , 0.809012f,-0.525738f),
SimdVector3(0.951058f , 0.309013f,0.000000f),
SimdVector3(0.951058f , -0.309013f,0.000000f),
SimdVector3(0.587786f , -0.809017f,0.000000f),
SimdVector3(0.000000f , -1.000000f,0.000000f),
SimdVector3(-0.587786f , -0.809017f,0.000000f),
SimdVector3(-0.951058f , -0.309013f,-0.000000f),
SimdVector3(-0.951058f , 0.309013f,-0.000000f),
SimdVector3(-0.587786f , 0.809017f,-0.000000f),
SimdVector3(-0.000000f , 1.000000f,-0.000000f),
SimdVector3(0.587786f , 0.809017f,-0.000000f),
SimdVector3(0.688190f , -0.499997f,0.525736f),
SimdVector3(-0.262869f , -0.809012f,0.525738f),
SimdVector3(-0.850648f , 0.000000f,0.525736f),
SimdVector3(-0.262869f , 0.809012f,0.525738f),
SimdVector3(0.688190f , 0.499997f,0.525736f),
SimdVector3(0.525730f , 0.000000f,0.850652f),
SimdVector3(0.162456f , -0.499995f,0.850654f),
SimdVector3(-0.425323f , -0.309011f,0.850654f),
SimdVector3(-0.425323f , 0.309011f,0.850654f),
SimdVector3(0.162456f , 0.499995f,0.850654f)
};


bool MinkowskiPenetrationDepthSolver::CalcPenDepth(SimplexSolverInterface& simplexSolver,
												   ConvexShape* convexA,ConvexShape* convexB,
												   const SimdTransform& transA,const SimdTransform& transB,
												   SimdVector3& v, SimdPoint3& pa, SimdPoint3& pb,
												   class IDebugDraw* debugDraw
												   )
{

	//just take fixed number of orientation, and sample the penetration depth in that direction
	float minProj = 1e30f;
	SimdVector3 minNorm;
	SimdVector3 minVertex;
	SimdVector3 minA,minB;
	SimdVector3 seperatingAxisInA,seperatingAxisInB;
	SimdVector3 pInA,qInB,pWorld,qWorld,w;

#define USE_BATCHED_SUPPORT 1
#ifdef USE_BATCHED_SUPPORT
	SimdVector3	supportVerticesABatch[NUM_UNITSPHERE_POINTS];
	SimdVector3	supportVerticesBBatch[NUM_UNITSPHERE_POINTS];
	SimdVector3	seperatingAxisInABatch[NUM_UNITSPHERE_POINTS];
	SimdVector3	seperatingAxisInBBatch[NUM_UNITSPHERE_POINTS];
	int i;

	for (i=0;i<NUM_UNITSPHERE_POINTS;i++)
	{
		const SimdVector3& norm = sPenetrationDirections[i];
		seperatingAxisInABatch[i] = (-norm)* transA.getBasis();
		seperatingAxisInBBatch[i] = norm * transB.getBasis();
	}

	convexA->BatchedUnitVectorGetSupportingVertexWithoutMargin(seperatingAxisInABatch,supportVerticesABatch,NUM_UNITSPHERE_POINTS);
	convexB->BatchedUnitVectorGetSupportingVertexWithoutMargin(seperatingAxisInBBatch,supportVerticesBBatch,NUM_UNITSPHERE_POINTS);
	for (i=0;i<NUM_UNITSPHERE_POINTS;i++)
	{
		const SimdVector3& norm = sPenetrationDirections[i];
		seperatingAxisInA = seperatingAxisInABatch[i];
		seperatingAxisInB = seperatingAxisInBBatch[i];

		pInA = supportVerticesABatch[i];
		qInB = supportVerticesBBatch[i];

		pWorld = transA(pInA);	
		qWorld = transB(qInB);
		w	= qWorld - pWorld;
		float delta = norm.dot(w);
		//find smallest delta
		if (delta < minProj)
		{
			minProj = delta;
			minNorm = norm;
			minA = pWorld;
			minB = qWorld;
		}
	}	
#else
	for (int i=0;i<NUM_UNITSPHERE_POINTS;i++)
	{
		const SimdVector3& norm = sPenetrationDirections[i];
		seperatingAxisInA = (-norm)* transA.getBasis();
		seperatingAxisInB = norm* transB.getBasis();
		pInA = convexA->LocalGetSupportingVertexWithoutMargin(seperatingAxisInA);
		qInB = convexB->LocalGetSupportingVertexWithoutMargin(seperatingAxisInB);
		pWorld = transA(pInA);	
		qWorld = transB(qInB);
		w	= qWorld - pWorld;
		float delta = norm.dot(w);
		//find smallest delta
		if (delta < minProj)
		{
			minProj = delta;
			minNorm = norm;
			minA = pWorld;
			minB = qWorld;
		}
	}
#endif //USE_BATCHED_SUPPORT

	//add the margins

	minA += minNorm*convexA->GetMargin();
	minB -= minNorm*convexB->GetMargin();
	minProj += (convexA->GetMargin() + convexB->GetMargin());


	

//#define DEBUG_DRAW 1
#ifdef DEBUG_DRAW
	if (debugDraw)
	{
		SimdVector3 color(0,1,0);
		debugDraw->DrawLine(minA,minB,color);
		color = SimdVector3 (1,1,1);
		SimdVector3 vec = minB-minA;
		float prj2 = minNorm.dot(vec);
		debugDraw->DrawLine(minA,minA+(minNorm*minProj),color);

	}
#endif //DEBUG_DRAW

	

	GjkPairDetector gjkdet(convexA,convexB,&simplexSolver,0);

	SimdScalar offsetDist = minProj;
	SimdVector3 offset = minNorm * offsetDist;
	


	GjkPairDetector::ClosestPointInput input;
		
	SimdVector3 newOrg = transA.getOrigin() + offset;

	SimdTransform displacedTrans = transA;
	displacedTrans.setOrigin(newOrg);

	input.m_transformA = displacedTrans;
	input.m_transformB = transB;
	input.m_maximumDistanceSquared = 1e30f;//minProj;
	
	MyResult res;
	gjkdet.GetClosestPoints(input,res,debugDraw);

	float correctedMinNorm = minProj - res.m_depth;


	//the penetration depth is over-estimated, relax it
	float penetration_relaxation= 1.f;
	minNorm*=penetration_relaxation;

	if (res.m_hasResult)
	{

		pa = res.m_pointInWorld - minNorm * correctedMinNorm;
		pb = res.m_pointInWorld;
		
#ifdef DEBUG_DRAW
		if (debugDraw)
		{
			SimdVector3 color(1,0,0);
			debugDraw->DrawLine(pa,pb,color);
		}
#endif//DEBUG_DRAW


	}
	return res.m_hasResult;
}



