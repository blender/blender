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



#include "btGjkConvexCast.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"
#include "BulletCollision/CollisionShapes/btMinkowskiSumShape.h"
#include "btGjkPairDetector.h"
#include "btPointCollector.h"


btGjkConvexCast::btGjkConvexCast(btConvexShape* convexA,btConvexShape* convexB,btSimplexSolverInterface* simplexSolver)
:m_simplexSolver(simplexSolver),
m_convexA(convexA),
m_convexB(convexB)
{
}

bool	btGjkConvexCast::calcTimeOfImpact(
					const btTransform& fromA,
					const btTransform& toA,
					const btTransform& fromB,
					const btTransform& toB,
					CastResult& result)
{


	btMinkowskiSumShape combi(m_convexA,m_convexB);
	btMinkowskiSumShape* convex = &combi;

	btTransform	rayFromLocalA;
	btTransform	rayToLocalA;

	rayFromLocalA = fromA.inverse()* fromB;
	rayToLocalA = toA.inverse()* toB;


	btTransform trA,trB;
	trA = btTransform(fromA);
	trB = btTransform(fromB);
	trA.setOrigin(btPoint3(0,0,0));
	trB.setOrigin(btPoint3(0,0,0));

	convex->setTransformA(trA);
	convex->setTransformB(trB);




	btScalar radius = btScalar(0.01);

	btScalar lambda = btScalar(0.);
	btVector3 s = rayFromLocalA.getOrigin();
	btVector3 r = rayToLocalA.getOrigin()-rayFromLocalA.getOrigin();
	btVector3 x = s;
	btVector3 n;
	n.setValue(0,0,0);
	bool hasResult = false;
	btVector3 c;

	btScalar lastLambda = lambda;

	//first solution, using GJK

	//no penetration support for now, perhaps pass a pointer when we really want it
	btConvexPenetrationDepthSolver* penSolverPtr = 0;

	btTransform identityTrans;
	identityTrans.setIdentity();

	btSphereShape	raySphere(btScalar(0.0));
	raySphere.setMargin(btScalar(0.));

	btTransform sphereTr;
	sphereTr.setIdentity();
	sphereTr.setOrigin( rayFromLocalA.getOrigin());

	result.drawCoordSystem(sphereTr);
	{
		btPointCollector	pointCollector1;
		btGjkPairDetector gjk(&raySphere,convex,m_simplexSolver,penSolverPtr);		

		btGjkPairDetector::ClosestPointInput input;
		input.m_transformA = sphereTr;
		input.m_transformB = identityTrans;
		gjk.getClosestPoints(input,pointCollector1,0);

		hasResult = pointCollector1.m_hasResult;
		c = pointCollector1.m_pointInWorld;
		n = pointCollector1.m_normalOnBInWorld;
	}

	

	if (hasResult)
	{
		btScalar dist;
		dist = (c-x).length();
		if (dist < radius)
		{
			//penetration
			lastLambda = btScalar(1.);
		}

		//not close enough
		while (dist > radius)
		{
			
			n = x - c;
			btScalar nDotr = n.dot(r);

			if (nDotr >= -(SIMD_EPSILON*SIMD_EPSILON))
				return false;
			
			lambda = lambda - n.dot(n) / nDotr;
			if (lambda <= lastLambda)
				break;

			lastLambda = lambda;

			x = s + lambda * r;

			sphereTr.setOrigin( x );
			result.drawCoordSystem(sphereTr);
			btPointCollector	pointCollector;
			btGjkPairDetector gjk(&raySphere,convex,m_simplexSolver,penSolverPtr);
			btGjkPairDetector::ClosestPointInput input;
			input.m_transformA = sphereTr;
			input.m_transformB = identityTrans;
			gjk.getClosestPoints(input,pointCollector,0);
			if (pointCollector.m_hasResult)
			{
				if (pointCollector.m_distance < btScalar(0.))
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

		if (lastLambda < btScalar(1.))
		{
		
			result.m_fraction = lastLambda;
			result.m_normal = n;
			return true;
		}
	}

	return false;
}

