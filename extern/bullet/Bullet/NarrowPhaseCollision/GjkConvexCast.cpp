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


#include "GjkConvexCast.h"
#include "CollisionShapes/SphereShape.h"
#include "CollisionShapes/MinkowskiSumShape.h"
#include "GjkPairDetector.h"
#include "PointCollector.h"


GjkConvexCast::GjkConvexCast(ConvexShape* convexA,ConvexShape* convexB,SimplexSolverInterface* simplexSolver)
:m_simplexSolver(simplexSolver),
m_convexA(convexA),
m_convexB(convexB)
{
}

bool	GjkConvexCast::calcTimeOfImpact(
					const SimdTransform& fromA,
					const SimdTransform& toA,
					const SimdTransform& fromB,
					const SimdTransform& toB,
					CastResult& result)
{


	MinkowskiSumShape combi(m_convexA,m_convexB);
	MinkowskiSumShape* convex = &combi;

	SimdTransform	rayFromLocalA;
	SimdTransform	rayToLocalA;

	rayFromLocalA = fromA.inverse()* fromB;
	rayToLocalA = toA.inverse()* toB;


	SimdTransform trA,trB;
	trA = SimdTransform(fromA);
	trB = SimdTransform(fromB);
	trA.setOrigin(SimdPoint3(0,0,0));
	trB.setOrigin(SimdPoint3(0,0,0));

	convex->SetTransformA(trA);
	convex->SetTransformB(trB);




	float radius = 0.01f;

	SimdScalar lambda = 0.f;
	SimdVector3 s = rayFromLocalA.getOrigin();
	SimdVector3 r = rayToLocalA.getOrigin()-rayFromLocalA.getOrigin();
	SimdVector3 x = s;
	SimdVector3 n;
	n.setValue(0,0,0);
	bool hasResult = false;
	SimdVector3 c;

	float lastLambda = lambda;

	//first solution, using GJK

	//no penetration support for now, perhaps pass a pointer when we really want it
	ConvexPenetrationDepthSolver* penSolverPtr = 0;

	SimdTransform identityTrans;
	identityTrans.setIdentity();

	SphereShape	raySphere(0.0f);
	raySphere.SetMargin(0.f);

	SimdTransform sphereTr;
	sphereTr.setIdentity();
	sphereTr.setOrigin( rayFromLocalA.getOrigin());

	result.DrawCoordSystem(sphereTr);
	{
		PointCollector	pointCollector1;
		GjkPairDetector gjk(&raySphere,convex,m_simplexSolver,penSolverPtr);		

		GjkPairDetector::ClosestPointInput input;
		input.m_transformA = sphereTr;
		input.m_transformB = identityTrans;
		gjk.GetClosestPoints(input,pointCollector1);

		hasResult = pointCollector1.m_hasResult;
		c = pointCollector1.m_pointInWorld;
		n = pointCollector1.m_normalOnBInWorld;
	}

	if (hasResult)
	{
		SimdScalar dist;
		dist = (c-x).length();
		

		//not close enough
		while (dist > radius)
		{
			
			n = x - c;
			SimdScalar nDotr = n.dot(r);

			if (nDotr >= 0.f)
				return false;

			lambda = lambda - n.dot(n) / nDotr;
			if (lambda <= lastLambda)
				break;
			lastLambda = lambda;

			x = s + lambda * r;

			sphereTr.setOrigin( x );
			result.DrawCoordSystem(sphereTr);
			PointCollector	pointCollector;
			GjkPairDetector gjk(&raySphere,convex,m_simplexSolver,penSolverPtr);
			GjkPairDetector::ClosestPointInput input;
			input.m_transformA = sphereTr;
			input.m_transformB = identityTrans;
			gjk.GetClosestPoints(input,pointCollector);
			if (pointCollector.m_hasResult)
			{
				if (pointCollector.m_distance < 0.f)
				{
					//degeneracy, report a hit
					result.m_fraction = lastLambda;
					result.m_normal = n;
					return true;
				}
				c = pointCollector.m_pointInWorld;			
				dist = (c-x).length();
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
}