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
		gjk.GetClosestPoints(input,pointCollector1,0);

		hasResult = pointCollector1.m_hasResult;
		c = pointCollector1.m_pointInWorld;
		n = pointCollector1.m_normalOnBInWorld;
	}

	

	if (hasResult)
	{
		SimdScalar dist;
		dist = (c-x).length();
		if (dist < radius)
		{
			//penetration
			lastLambda = 1.f;
		}

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
			gjk.GetClosestPoints(input,pointCollector,0);
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

		if (lastLambda < 1.f)
		{
		
			result.m_fraction = lastLambda;
			result.m_normal = n;
			return true;
		}
	}

	return false;
}

