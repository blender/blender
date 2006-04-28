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
	//float epsilon = 0.001f;

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
	
		//we don't use margins during CCD
		gjk.SetIgnoreMargin(true);

		input.m_transformA = fromA;
		input.m_transformB = fromB;
		gjk.GetClosestPoints(input,pointCollector1,0);

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
			//float clippedDist  = dist;
			
			float projectedLinearVelocity = (linVelB-linVelA).dot(n);
			
			dLambda = dist / (projectedLinearVelocity+ maxAngularProjectedVelocity);

			lambda = lambda + dLambda;

			if (lambda > 1.f)
				return false;

			if (lambda < 0.f)
				return false;

			//todo: next check with relative epsilon
			if (lambda <= lastLambda)
				break;
			lastLambda = lambda;

			

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
			gjk.GetClosestPoints(input,pointCollector,0);
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

