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


#include "btSubSimplexConvexCast.h"
#include "BulletCollision/CollisionShapes/btConvexShape.h"
#include "BulletCollision/CollisionShapes/btMinkowskiSumShape.h"
#include "BulletCollision/NarrowPhaseCollision/btSimplexSolverInterface.h"


btSubsimplexConvexCast::btSubsimplexConvexCast (const btConvexShape* convexA,const btConvexShape* convexB,btSimplexSolverInterface* simplexSolver)
:m_simplexSolver(simplexSolver),
m_convexA(convexA),m_convexB(convexB)
{
}

///Typically the conservative advancement reaches solution in a few iterations, clip it to 32 for degenerate cases.
///See discussion about this here http://continuousphysics.com/Bullet/phpBB2/viewtopic.php?t=565
#ifdef BT_USE_DOUBLE_PRECISION
#define MAX_ITERATIONS 64
#else
#define MAX_ITERATIONS 32
#endif
bool	btSubsimplexConvexCast::calcTimeOfImpact(
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


	m_simplexSolver->reset();

	convex->setTransformB(btTransform(rayFromLocalA.getBasis()));

	//btScalar radius = btScalar(0.01);

	btScalar lambda = btScalar(0.);
	//todo: need to verify this:
	//because of minkowski difference, we need the inverse direction
	
	btVector3 s = -rayFromLocalA.getOrigin();
	btVector3 r = -(rayToLocalA.getOrigin()-rayFromLocalA.getOrigin());
	btVector3 x = s;
	btVector3 v;
	btVector3 arbitraryPoint = convex->localGetSupportingVertex(r);
	
	v = x - arbitraryPoint;

	int maxIter = MAX_ITERATIONS;

	btVector3 n;
	n.setValue(btScalar(0.),btScalar(0.),btScalar(0.));
	bool hasResult = false;
	btVector3 c;

	btScalar lastLambda = lambda;


	btScalar dist2 = v.length2();
#ifdef BT_USE_DOUBLE_PRECISION
	btScalar epsilon = btScalar(0.0001);
#else
	btScalar epsilon = btScalar(0.0001);
#endif //BT_USE_DOUBLE_PRECISION
	btVector3	w,p;
	btScalar VdotR;
	
	while ( (dist2 > epsilon) && maxIter--)
	{
		p = convex->localGetSupportingVertex( v);
		 w = x - p;

		btScalar VdotW = v.dot(w);

		if ( VdotW > btScalar(0.))
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
			dist2 = btScalar(0.);
		} 
	}

	//int numiter = MAX_ITERATIONS - maxIter;
//	printf("number of iterations: %d", numiter);
	result.m_fraction = lambda;
	result.m_normal = n;

	return true;
}



