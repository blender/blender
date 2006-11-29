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

#include "btMinkowskiPenetrationDepthSolver.h"
#include "BulletCollision/CollisionShapes/btMinkowskiSumShape.h"
#include "BulletCollision/NarrowPhaseCollision/btSubSimplexConvexCast.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h"




#define NUM_UNITSPHERE_POINTS 42
static btVector3	sPenetrationDirections[NUM_UNITSPHERE_POINTS+MAX_PREFERRED_PENETRATION_DIRECTIONS*2] = 
{
btVector3(0.000000f , -0.000000f,-1.000000f),
btVector3(0.723608f , -0.525725f,-0.447219f),
btVector3(-0.276388f , -0.850649f,-0.447219f),
btVector3(-0.894426f , -0.000000f,-0.447216f),
btVector3(-0.276388f , 0.850649f,-0.447220f),
btVector3(0.723608f , 0.525725f,-0.447219f),
btVector3(0.276388f , -0.850649f,0.447220f),
btVector3(-0.723608f , -0.525725f,0.447219f),
btVector3(-0.723608f , 0.525725f,0.447219f),
btVector3(0.276388f , 0.850649f,0.447219f),
btVector3(0.894426f , 0.000000f,0.447216f),
btVector3(-0.000000f , 0.000000f,1.000000f),
btVector3(0.425323f , -0.309011f,-0.850654f),
btVector3(-0.162456f , -0.499995f,-0.850654f),
btVector3(0.262869f , -0.809012f,-0.525738f),
btVector3(0.425323f , 0.309011f,-0.850654f),
btVector3(0.850648f , -0.000000f,-0.525736f),
btVector3(-0.525730f , -0.000000f,-0.850652f),
btVector3(-0.688190f , -0.499997f,-0.525736f),
btVector3(-0.162456f , 0.499995f,-0.850654f),
btVector3(-0.688190f , 0.499997f,-0.525736f),
btVector3(0.262869f , 0.809012f,-0.525738f),
btVector3(0.951058f , 0.309013f,0.000000f),
btVector3(0.951058f , -0.309013f,0.000000f),
btVector3(0.587786f , -0.809017f,0.000000f),
btVector3(0.000000f , -1.000000f,0.000000f),
btVector3(-0.587786f , -0.809017f,0.000000f),
btVector3(-0.951058f , -0.309013f,-0.000000f),
btVector3(-0.951058f , 0.309013f,-0.000000f),
btVector3(-0.587786f , 0.809017f,-0.000000f),
btVector3(-0.000000f , 1.000000f,-0.000000f),
btVector3(0.587786f , 0.809017f,-0.000000f),
btVector3(0.688190f , -0.499997f,0.525736f),
btVector3(-0.262869f , -0.809012f,0.525738f),
btVector3(-0.850648f , 0.000000f,0.525736f),
btVector3(-0.262869f , 0.809012f,0.525738f),
btVector3(0.688190f , 0.499997f,0.525736f),
btVector3(0.525730f , 0.000000f,0.850652f),
btVector3(0.162456f , -0.499995f,0.850654f),
btVector3(-0.425323f , -0.309011f,0.850654f),
btVector3(-0.425323f , 0.309011f,0.850654f),
btVector3(0.162456f , 0.499995f,0.850654f)
};


bool btMinkowskiPenetrationDepthSolver::calcPenDepth(btSimplexSolverInterface& simplexSolver,
												   btConvexShape* convexA,btConvexShape* convexB,
												   const btTransform& transA,const btTransform& transB,
												   btVector3& v, btPoint3& pa, btPoint3& pb,
												   class btIDebugDraw* debugDraw,btStackAlloc* stackAlloc
												   )
{


	struct btIntermediateResult : public btDiscreteCollisionDetectorInterface::Result
	{

		btIntermediateResult():m_hasResult(false)
		{
		}
		
		btVector3 m_normalOnBInWorld;
		btVector3 m_pointInWorld;
		float m_depth;
		bool	m_hasResult;

		virtual void setShapeIdentifiers(int partId0,int index0,	int partId1,int index1)
		{
		}
		void addContactPoint(const btVector3& normalOnBInWorld,const btVector3& pointInWorld,float depth)
		{
			m_normalOnBInWorld = normalOnBInWorld;
			m_pointInWorld = pointInWorld;
			m_depth = depth;
			m_hasResult = true;
		}
	};

	//just take fixed number of orientation, and sample the penetration depth in that direction
	float minProj = 1e30f;
	btVector3 minNorm;
	btVector3 minVertex;
	btVector3 minA,minB;
	btVector3 seperatingAxisInA,seperatingAxisInB;
	btVector3 pInA,qInB,pWorld,qWorld,w;

#define USE_BATCHED_SUPPORT 1
#ifdef USE_BATCHED_SUPPORT

	btVector3	supportVerticesABatch[NUM_UNITSPHERE_POINTS+MAX_PREFERRED_PENETRATION_DIRECTIONS*2];
	btVector3	supportVerticesBBatch[NUM_UNITSPHERE_POINTS+MAX_PREFERRED_PENETRATION_DIRECTIONS*2];
	btVector3	seperatingAxisInABatch[NUM_UNITSPHERE_POINTS+MAX_PREFERRED_PENETRATION_DIRECTIONS*2];
	btVector3	seperatingAxisInBBatch[NUM_UNITSPHERE_POINTS+MAX_PREFERRED_PENETRATION_DIRECTIONS*2];
	int i;

	int numSampleDirections = NUM_UNITSPHERE_POINTS;

	for (i=0;i<numSampleDirections;i++)
	{
		const btVector3& norm = sPenetrationDirections[i];
		seperatingAxisInABatch[i] =  (-norm) * transA.getBasis() ;
		seperatingAxisInBBatch[i] =  norm   * transB.getBasis() ;
	}

	{
		int numPDA = convexA->getNumPreferredPenetrationDirections();
		if (numPDA)
		{
			for (int i=0;i<numPDA;i++)
			{
				btVector3 norm;
				convexA->getPreferredPenetrationDirection(i,norm);
				norm  = transA.getBasis() * norm;
				sPenetrationDirections[numSampleDirections] = norm;
				seperatingAxisInABatch[numSampleDirections] = (-norm) * transA.getBasis();
				seperatingAxisInBBatch[numSampleDirections] = norm * transB.getBasis();
				numSampleDirections++;
			}
		}
	}

	{
		int numPDB = convexB->getNumPreferredPenetrationDirections();
		if (numPDB)
		{
			for (int i=0;i<numPDB;i++)
			{
				btVector3 norm;
				convexB->getPreferredPenetrationDirection(i,norm);
				norm  = transB.getBasis() * norm;
				sPenetrationDirections[numSampleDirections] = norm;
				seperatingAxisInABatch[numSampleDirections] = (-norm) * transA.getBasis();
				seperatingAxisInBBatch[numSampleDirections] = norm * transB.getBasis();
				numSampleDirections++;
			}
		}
	}



	convexA->batchedUnitVectorGetSupportingVertexWithoutMargin(seperatingAxisInABatch,supportVerticesABatch,numSampleDirections);
	convexB->batchedUnitVectorGetSupportingVertexWithoutMargin(seperatingAxisInBBatch,supportVerticesBBatch,numSampleDirections);

	for (i=0;i<numSampleDirections;i++)
	{
		const btVector3& norm = sPenetrationDirections[i];
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

	int numSampleDirections = NUM_UNITSPHERE_POINTS;

	{
		int numPDA = convexA->getNumPreferredPenetrationDirections();
		if (numPDA)
		{
			for (int i=0;i<numPDA;i++)
			{
				btVector3 norm;
				convexA->getPreferredPenetrationDirection(i,norm);
				norm  = transA.getBasis() * norm;
				sPenetrationDirections[numSampleDirections] = norm;
				numSampleDirections++;
			}
		}
	}

	{
		int numPDB = convexB->getNumPreferredPenetrationDirections();
		if (numPDB)
		{
			for (int i=0;i<numPDB;i++)
			{
				btVector3 norm;
				convexB->getPreferredPenetrationDirection(i,norm);
				norm  = transB.getBasis() * norm;
				sPenetrationDirections[numSampleDirections] = norm;
				numSampleDirections++;
			}
		}
	}

	for (int i=0;i<numSampleDirections;i++)
	{
		const btVector3& norm = sPenetrationDirections[i];
		seperatingAxisInA = (-norm)* transA.getBasis();
		seperatingAxisInB = norm* transB.getBasis();
		pInA = convexA->localGetSupportingVertexWithoutMargin(seperatingAxisInA);
		qInB = convexB->localGetSupportingVertexWithoutMargin(seperatingAxisInB);
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

	minA += minNorm*convexA->getMargin();
	minB -= minNorm*convexB->getMargin();
	//no penetration
	if (minProj < 0.f)
		return false;

	minProj += (convexA->getMargin() + convexB->getMargin());





//#define DEBUG_DRAW 1
#ifdef DEBUG_DRAW
	if (debugDraw)
	{
		btVector3 color(0,1,0);
		debugDraw->drawLine(minA,minB,color);
		color = btVector3 (1,1,1);
		btVector3 vec = minB-minA;
		float prj2 = minNorm.dot(vec);
		debugDraw->drawLine(minA,minA+(minNorm*minProj),color);

	}
#endif //DEBUG_DRAW

	

	btGjkPairDetector gjkdet(convexA,convexB,&simplexSolver,0);

	btScalar offsetDist = minProj;
	btVector3 offset = minNorm * offsetDist;
	


	btGjkPairDetector::ClosestPointInput input;
		
	btVector3 newOrg = transA.getOrigin() + offset;

	btTransform displacedTrans = transA;
	displacedTrans.setOrigin(newOrg);

	input.m_transformA = displacedTrans;
	input.m_transformB = transB;
	input.m_maximumDistanceSquared = 1e30f;//minProj;
	
	btIntermediateResult res;
	gjkdet.getClosestPoints(input,res,debugDraw);

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
			btVector3 color(1,0,0);
			debugDraw->drawLine(pa,pb,color);
		}
#endif//DEBUG_DRAW


	}
	return res.m_hasResult;
}



