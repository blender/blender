#include "LinearMath/btVector3.h"
#include "LinearMath/btScalar.h"	
#include "LinearMath/btMatrix3x3.h"
#include "LinearMath/btTransform.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include "BulletCollision/CollisionShapes/btTriangleShape.h"

#include "BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h"
#include "BulletCollision/NarrowPhaseCollision/btPointCollector.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btSubSimplexConvexCast.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpa.h"
#include "BulletCollision/CollisionShapes/btMinkowskiSumShape.h"
#include "BulletCollision/NarrowPhaseCollision/btSubSimplexConvexCast.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h"

#include "BulletCollision/NarrowPhaseCollision/btDiscreteCollisionDetectorInterface.h"
#include "BulletCollision/NarrowPhaseCollision/btSimplexSolverInterface.h"
#include "BulletCollision/NarrowPhaseCollision/btMinkowskiPenetrationDepthSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h"
#include "LinearMath/btStackAlloc.h"


extern "C"
double plNearestPoints(float p[3][3], float q[3][3], float *pa, float *pb, float normal[3])
{
	btTriangleShape trishapeA(btVector3(p[0][0], p[0][1], p[0][2]), btVector3(p[1][0], p[1][1], p[1][2]), btVector3(p[2][0], p[2][1], p[2][2]));
	trishapeA.setMargin(0.001f);
	
	btTriangleShape trishapeB(btVector3(q[0][0], q[0][1], q[0][2]), btVector3(q[1][0], q[1][1], q[1][2]), btVector3(q[2][0], q[2][1], q[2][2]));
	trishapeB.setMargin(0.001f);
	
	// btVoronoiSimplexSolver sGjkSimplexSolver;
	// btGjkEpaPenetrationDepthSolver penSolverPtr;	
	
	static btSimplexSolverInterface sGjkSimplexSolver;
	sGjkSimplexSolver.reset();
	
	static btGjkEpaPenetrationDepthSolver Solver0;
	static btMinkowskiPenetrationDepthSolver Solver1;
		
	btConvexPenetrationDepthSolver* Solver = NULL;
	
	Solver = &Solver0;	
		
	btGjkPairDetector convexConvex(&trishapeA ,&trishapeB,&sGjkSimplexSolver,Solver);
	
	convexConvex.m_catchDegeneracies = 1;
	
	// btGjkPairDetector convexConvex(&trishapeA ,&trishapeB,&sGjkSimplexSolver,0);
	
	btPointCollector gjkOutput;
	btGjkPairDetector::ClosestPointInput input;
	
	btStackAlloc gStackAlloc(1024*1024*2);
 
	input.m_stackAlloc = &gStackAlloc;
	
	btTransform tr;
	tr.setIdentity();
	
	input.m_transformA = tr;
	input.m_transformB = tr;
	
	convexConvex.getClosestPoints(input, gjkOutput, 0);
	
	
	if (gjkOutput.m_hasResult)
	{
		
		pb[0] = pa[0] = gjkOutput.m_pointInWorld[0];
		pb[1] = pa[1] = gjkOutput.m_pointInWorld[1];
		pb[2] = pa[2] = gjkOutput.m_pointInWorld[2];

		pb[0]+= gjkOutput.m_normalOnBInWorld[0] * gjkOutput.m_distance;
		pb[1]+= gjkOutput.m_normalOnBInWorld[1] * gjkOutput.m_distance;
		pb[2]+= gjkOutput.m_normalOnBInWorld[2] * gjkOutput.m_distance;
		
		normal[0] = gjkOutput.m_normalOnBInWorld[0];
		normal[1] = gjkOutput.m_normalOnBInWorld[1];
		normal[2] = gjkOutput.m_normalOnBInWorld[2];

		return gjkOutput.m_distance;
	}
	return -1.0f;	
}

 
