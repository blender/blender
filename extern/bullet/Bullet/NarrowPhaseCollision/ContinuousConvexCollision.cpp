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

#include "ContinuousConvexCollision.h"
#include "CollisionShapes/ConvexShape.h"
#include "CollisionShapes/MinkowskiSumShape.h"
#include "NarrowPhaseCollision/SimplexSolverInterface.h"
#include "SimdTransformUtil.h"
#include "CollisionShapes/SphereShape.h"

#include "GjkPairDetector.h"
#include "PointCollector.h"



ContinuousConvexCollision::ContinuousConvexCollision ( ConvexShape*	convexA,ConvexShape*	convexB,SimplexSolverInterface* simplexSolver, ConvexPenetrationDepthSolver* penetrationDepthSolver)
:m_simplexSolver(simplexSolver),
m_penetrationDepthSolver(penetrationDepthSolver),
m_convexA(convexA),m_convexB(convexB)
{
}

/// This maximum should not be necessary. It allows for untested/degenerate cases in production code.
/// You don't want your game ever to lock-up.
#define MAX_ITERATIONS 1000

bool	ContinuousConvexCollision::calcTimeOfImpact(
				const SimdTransform& fromA,
				const SimdTransform& toA,
				const SimdTransform& fromB,
				const SimdTransform& toB,
				CastResult& result)
{

	m_simplexSolver->reset();

	/// compute linear and angular velocity for this interval, to interpolate
	SimdVector3 linVelA,angVelA,linVelB,angVelB;
	SimdTransformUtil::CalculateVelocity(fromA,toA,1.f,linVelA,angVelA);
	SimdTransformUtil::CalculateVelocity(fromB,toB,1.f,linVelB,angVelB);

	SimdScalar boundingRadiusA = m_convexA->GetAngularMotionDisc();
	SimdScalar boundingRadiusB = m_convexB->GetAngularMotionDisc();

	SimdScalar maxAngularProjectedVelocity = angVelA.length() * boundingRadiusA + angVelB.length() * boundingRadiusB;

	float radius = 0.001f;

	SimdScalar lambda = 0.f;
	SimdVector3 v(1,0,0);

	int maxIter = MAX_ITERATIONS;

	SimdVector3 n;
	n.setValue(0.f,0.f,0.f);
	bool hasResult = false;
	SimdVector3 c;

	float lastLambda = lambda;
	float epsilon = 0.001f;

	int numIter = 0;
	//first solution, using GJK


	SimdTransform identityTrans;
	identityTrans.setIdentity();

	SphereShape	raySphere(0.0f);
	raySphere.SetMargin(0.f);


//	result.DrawCoordSystem(sphereTr);

	PointCollector	pointCollector1;

	{
		
		GjkPairDetector gjk(m_convexA,m_convexB,m_simplexSolver,m_penetrationDepthSolver);		
		GjkPairDetector::ClosestPointInput input;
		input.m_transformA = fromA;
		input.m_transformB = fromB;
		gjk.GetClosestPoints(input,pointCollector1);

		hasResult = pointCollector1.m_hasResult;
		c = pointCollector1.m_pointInWorld;
	}

	if (hasResult)
	{
		SimdScalar dist;
		dist = pointCollector1.m_distance;
		n = pointCollector1.m_normalOnBInWorld;
		
		//not close enough
		while (dist > radius)
		{
			numIter++;
			if (numIter > maxIter)
				return false; //todo: report a failure

			float dLambda = 0.f;

			//calculate safe moving fraction from distance / (linear+rotational velocity)
			
			//float clippedDist  = GEN_min(angularConservativeRadius,dist);
			float clippedDist  = dist;
			
			float projectedLinearVelocity = (linVelB-linVelA).dot(n);
			
			dLambda = dist / (projectedLinearVelocity+ maxAngularProjectedVelocity);

			lambda = lambda + dLambda;

			//todo: next check with relative epsilon
			if (lambda <= lastLambda)
				break;
			lastLambda = lambda;

			if (lambda > 1.f)
				return false;

			//interpolate to next lambda
			SimdTransform interpolatedTransA,interpolatedTransB,relativeTrans;

			SimdTransformUtil::IntegrateTransform(fromA,linVelA,angVelA,lambda,interpolatedTransA);
			SimdTransformUtil::IntegrateTransform(fromB,linVelB,angVelB,lambda,interpolatedTransB);
			relativeTrans = interpolatedTransB.inverseTimes(interpolatedTransA);

			result.DebugDraw( lambda );

			PointCollector	pointCollector;
			GjkPairDetector gjk(m_convexA,m_convexB,m_simplexSolver,m_penetrationDepthSolver);
			GjkPairDetector::ClosestPointInput input;
			input.m_transformA = interpolatedTransA;
			input.m_transformB = interpolatedTransB;
			gjk.GetClosestPoints(input,pointCollector);
			if (pointCollector.m_hasResult)
			{
				if (pointCollector.m_distance < 0.f)
				{
					//degenerate ?!
					result.m_fraction = lastLambda;
					result.m_normal = n;
					return true;
				}
				c = pointCollector.m_pointInWorld;		
				
				dist = pointCollector.m_distance;
			} else
			{
				//??
				return false;
			}

		}

		result.m_fraction = lambda;
		result.m_normal = n;
		return true;
	}

	return false;

/*
//todo:
	//if movement away from normal, discard result
	SimdVector3 move = transBLocalTo.getOrigin() - transBLocalFrom.getOrigin();
	if (result.m_fraction < 1.f)
	{
		if (move.dot(result.m_normal) <= 0.f)
		{
		}
	}
*/

}

