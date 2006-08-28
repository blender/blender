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


#include "SubSimplexConvexCast.h"
#include "CollisionShapes/ConvexShape.h"
#include "CollisionShapes/MinkowskiSumShape.h"
#include "NarrowPhaseCollision/SimplexSolverInterface.h"


SubsimplexConvexCast::SubsimplexConvexCast (ConvexShape* convexA,ConvexShape* convexB,SimplexSolverInterface* simplexSolver)
:m_simplexSolver(simplexSolver),
m_convexA(convexA),m_convexB(convexB)
{
}


#define MAX_ITERATIONS 1000

bool	SubsimplexConvexCast::calcTimeOfImpact(
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


	m_simplexSolver->reset();

	convex->SetTransformB(SimdTransform(rayFromLocalA.getBasis()));

	//float radius = 0.01f;

	SimdScalar lambda = 0.f;
	//todo: need to verify this:
	//because of minkowski difference, we need the inverse direction
	
	SimdVector3 s = -rayFromLocalA.getOrigin();
	SimdVector3 r = -(rayToLocalA.getOrigin()-rayFromLocalA.getOrigin());
	SimdVector3 x = s;
	SimdVector3 v;
	SimdVector3 arbitraryPoint = convex->LocalGetSupportingVertex(r);
	
	v = x - arbitraryPoint;

	int maxIter = MAX_ITERATIONS;

	SimdVector3 n;
	n.setValue(0.f,0.f,0.f);
	bool hasResult = false;
	SimdVector3 c;

	float lastLambda = lambda;


	float dist2 = v.length2();
	float epsilon = 0.0001f;

	SimdVector3	w,p;
	float VdotR;
	
	while ( (dist2 > epsilon) && maxIter--)
	{
		p = convex->LocalGetSupportingVertex( v);
		 w = x - p;

		float VdotW = v.dot(w);

		if ( VdotW > 0.f)
		{
			VdotR = v.dot(r);

			if (VdotR >= -(SIMD_EPSILON*SIMD_EPSILON))
				return false;
			else
			{
				lambda = lambda - VdotW / VdotR;
				x = s + lambda * r;
				m_simplexSolver->reset();
				//check next line
				w = x-p;
				lastLambda = lambda;
				n = v;
				hasResult = true;
			}
		} 
		m_simplexSolver->addVertex( w, x , p);
		if (m_simplexSolver->closest(v))
		{
			dist2 = v.length2();
			hasResult = true;
			//printf("V=%f , %f, %f\n",v[0],v[1],v[2]);
			//printf("DIST2=%f\n",dist2);
			//printf("numverts = %i\n",m_simplexSolver->numVertices());
		} else
		{
			dist2 = 0.f;
		} 
	}

	//int numiter = MAX_ITERATIONS - maxIter;
//	printf("number of iterations: %d", numiter);
	result.m_fraction = lambda;
	result.m_normal = n;

	return true;
}



