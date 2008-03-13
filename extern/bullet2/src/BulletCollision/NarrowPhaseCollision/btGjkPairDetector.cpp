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

#include "btGjkPairDetector.h"
#include "BulletCollision/CollisionShapes/btConvexShape.h"
#include "BulletCollision/NarrowPhaseCollision/btSimplexSolverInterface.h"
#include "BulletCollision/NarrowPhaseCollision/btConvexPenetrationDepthSolver.h"

#if defined(DEBUG) || defined (_DEBUG)
#include <stdio.h> //for debug printf
#ifdef __SPU__
#include <spu_printf.h>
#define printf spu_printf
#endif //__SPU__
#endif

//must be above the machine epsilon
#define REL_ERROR2 btScalar(1.0e-6)

//temp globals, to improve GJK/EPA/penetration calculations
int gNumDeepPenetrationChecks = 0;
int gNumGjkChecks = 0;



btGjkPairDetector::btGjkPairDetector(btConvexShape* objectA,btConvexShape* objectB,btSimplexSolverInterface* simplexSolver,btConvexPenetrationDepthSolver*	penetrationDepthSolver)
:m_cachedSeparatingAxis(btScalar(0.),btScalar(0.),btScalar(1.)),
m_penetrationDepthSolver(penetrationDepthSolver),
m_simplexSolver(simplexSolver),
m_minkowskiA(objectA),
m_minkowskiB(objectB),
m_ignoreMargin(false),
m_lastUsedMethod(-1),
m_catchDegeneracies(1)
{
}

void btGjkPairDetector::getClosestPoints(const ClosestPointInput& input,Result& output,class btIDebugDraw* debugDraw)
{
	btScalar distance=btScalar(0.);
	btVector3	normalInB(btScalar(0.),btScalar(0.),btScalar(0.));
	btVector3 pointOnA,pointOnB;
	btTransform	localTransA = input.m_transformA;
	btTransform localTransB = input.m_transformB;
	btVector3 positionOffset = (localTransA.getOrigin() + localTransB.getOrigin()) * btScalar(0.5);
	localTransA.getOrigin() -= positionOffset;
	localTransB.getOrigin() -= positionOffset;

	btScalar marginA = m_minkowskiA->getMargin();
	btScalar marginB = m_minkowskiB->getMargin();

	gNumGjkChecks++;

	//for CCD we don't use margins
	if (m_ignoreMargin)
	{
		marginA = btScalar(0.);
		marginB = btScalar(0.);
	}

	m_curIter = 0;
	int gGjkMaxIter = 1000;//this is to catch invalid input, perhaps check for #NaN?
	m_cachedSeparatingAxis.setValue(0,1,0);

	bool isValid = false;
	bool checkSimplex = false;
	bool checkPenetration = true;
	m_degenerateSimplex = 0;

	m_lastUsedMethod = -1;

	{
		btScalar squaredDistance = SIMD_INFINITY;
		btScalar delta = btScalar(0.);
		
		btScalar margin = marginA + marginB;
		
		

		m_simplexSolver->reset();
		
		for ( ; ; )
		//while (true)
		{

			btVector3 seperatingAxisInA = (-m_cachedSeparatingAxis)* input.m_transformA.getBasis();
			btVector3 seperatingAxisInB = m_cachedSeparatingAxis* input.m_transformB.getBasis();

			btVector3 pInA = m_minkowskiA->localGetSupportingVertexWithoutMargin(seperatingAxisInA);
			btVector3 qInB = m_minkowskiB->localGetSupportingVertexWithoutMargin(seperatingAxisInB);
			btPoint3  pWorld = localTransA(pInA);	
			btPoint3  qWorld = localTransB(qInB);
			
			btVector3 w	= pWorld - qWorld;
			delta = m_cachedSeparatingAxis.dot(w);

			// potential exit, they don't overlap
			if ((delta > btScalar(0.0)) && (delta * delta > squaredDistance * input.m_maximumDistanceSquared)) 
			{
				checkPenetration = false;
				break;
			}

			//exit 0: the new point is already in the simplex, or we didn't come any closer
			if (m_simplexSolver->inSimplex(w))
			{
				m_degenerateSimplex = 1;
				checkSimplex = true;
				break;
			}
			// are we getting any closer ?
			btScalar f0 = squaredDistance - delta;
			btScalar f1 = squaredDistance * REL_ERROR2;

			if (f0 <= f1)
			{
				if (f0 <= btScalar(0.))
				{
					m_degenerateSimplex = 2;
				}
				checkSimplex = true;
				break;
			}
			//add current vertex to simplex
			m_simplexSolver->addVertex(w, pWorld, qWorld);

			//calculate the closest point to the origin (update vector v)
			if (!m_simplexSolver->closest(m_cachedSeparatingAxis))
			{
				m_degenerateSimplex = 3;
				checkSimplex = true;
				break;
			}

			btScalar previousSquaredDistance = squaredDistance;
			squaredDistance = m_cachedSeparatingAxis.length2();
			
			//redundant m_simplexSolver->compute_points(pointOnA, pointOnB);

			//are we getting any closer ?
			if (previousSquaredDistance - squaredDistance <= SIMD_EPSILON * previousSquaredDistance) 
			{ 
				m_simplexSolver->backup_closest(m_cachedSeparatingAxis);
				checkSimplex = true;
				break;
			}

			  //degeneracy, this is typically due to invalid/uninitialized worldtransforms for a btCollisionObject   
              if (m_curIter++ > gGjkMaxIter)   
              {   
                      #if defined(DEBUG) || defined (_DEBUG)   

                              printf("btGjkPairDetector maxIter exceeded:%i\n",m_curIter);   
                              printf("sepAxis=(%f,%f,%f), squaredDistance = %f, shapeTypeA=%i,shapeTypeB=%i\n",   
                              m_cachedSeparatingAxis.getX(),   
                              m_cachedSeparatingAxis.getY(),   
                              m_cachedSeparatingAxis.getZ(),   
                              squaredDistance,   
                              m_minkowskiA->getShapeType(),   
                              m_minkowskiB->getShapeType());   

                      #endif   
                      break;   

              } 


			bool check = (!m_simplexSolver->fullSimplex());
			//bool check = (!m_simplexSolver->fullSimplex() && squaredDistance > SIMD_EPSILON * m_simplexSolver->maxVertex());

			if (!check)
			{
				//do we need this backup_closest here ?
				m_simplexSolver->backup_closest(m_cachedSeparatingAxis);
				break;
			}
		}

		if (checkSimplex)
		{
			m_simplexSolver->compute_points(pointOnA, pointOnB);
			normalInB = pointOnA-pointOnB;
			btScalar lenSqr = m_cachedSeparatingAxis.length2();
			//valid normal
			if (lenSqr < 0.0001)
			{
				m_degenerateSimplex = 5;
			} 
			if (lenSqr > SIMD_EPSILON*SIMD_EPSILON)
			{
				btScalar rlen = btScalar(1.) / btSqrt(lenSqr );
				normalInB *= rlen; //normalize
				btScalar s = btSqrt(squaredDistance);
			
				btAssert(s > btScalar(0.0));
				pointOnA -= m_cachedSeparatingAxis * (marginA / s);
				pointOnB += m_cachedSeparatingAxis * (marginB / s);
				distance = ((btScalar(1.)/rlen) - margin);
				isValid = true;
				
				m_lastUsedMethod = 1;
			} else
			{
				m_lastUsedMethod = 2;
			}
		}

		bool catchDegeneratePenetrationCase = 
			(m_catchDegeneracies && m_penetrationDepthSolver && m_degenerateSimplex && ((distance+margin) < 0.01));

		//if (checkPenetration && !isValid)
		if (checkPenetration && (!isValid || catchDegeneratePenetrationCase ))
		{
			//penetration case
		
			//if there is no way to handle penetrations, bail out
			if (m_penetrationDepthSolver)
			{
				// Penetration depth case.
				btVector3 tmpPointOnA,tmpPointOnB;
				
				gNumDeepPenetrationChecks++;

				bool isValid2 = m_penetrationDepthSolver->calcPenDepth( 
					*m_simplexSolver, 
					m_minkowskiA,m_minkowskiB,
					localTransA,localTransB,
					m_cachedSeparatingAxis, tmpPointOnA, tmpPointOnB,
					debugDraw,input.m_stackAlloc
					);

				if (isValid2)
				{
					btVector3 tmpNormalInB = tmpPointOnB-tmpPointOnA;
					btScalar lenSqr = tmpNormalInB.length2();
					if (lenSqr > (SIMD_EPSILON*SIMD_EPSILON))
					{
						tmpNormalInB /= btSqrt(lenSqr);
						btScalar distance2 = -(tmpPointOnA-tmpPointOnB).length();
						//only replace valid penetrations when the result is deeper (check)
						if (!isValid || (distance2 < distance))
						{
							distance = distance2;
							pointOnA = tmpPointOnA;
							pointOnB = tmpPointOnB;
							normalInB = tmpNormalInB;
							isValid = true;
							m_lastUsedMethod = 3;
						} else
						{
							
						}
					} else
					{
						//isValid = false;
						m_lastUsedMethod = 4;
					}
				} else
				{
					m_lastUsedMethod = 5;
				}
				
			}
		}
	}

	if (isValid)
	{
#ifdef __SPU__
		//spu_printf("distance\n");
#endif //__CELLOS_LV2__


		output.addContactPoint(
			normalInB,
			pointOnB+positionOffset,
			distance);
		//printf("gjk add:%f",distance);
	}


}





