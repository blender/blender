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

#include "GjkPairDetector.h"
#include "CollisionShapes/ConvexShape.h"
#include "NarrowPhaseCollision/SimplexSolverInterface.h"
#include "NarrowPhaseCollision/ConvexPenetrationDepthSolver.h"

static const SimdScalar rel_error = SimdScalar(1.0e-3);
SimdScalar rel_error2 = rel_error * rel_error;
float maxdist2 = 1.e30f;



GjkPairDetector::GjkPairDetector(ConvexShape* objectA,ConvexShape* objectB,SimplexSolverInterface* simplexSolver,ConvexPenetrationDepthSolver*	penetrationDepthSolver)
:m_cachedSeparatingAxis(0.f,0.f,1.f),
m_penetrationDepthSolver(penetrationDepthSolver),
m_simplexSolver(simplexSolver),
m_minkowskiA(objectA),
m_minkowskiB(objectB)
{
}

void GjkPairDetector::GetClosestPoints(const ClosestPointInput& input,Result& output)
{
	SimdScalar distance;
	SimdVector3	normalInB(0.f,0.f,0.f);
	SimdVector3 pointOnA,pointOnB;

	float marginA = m_minkowskiA->GetMargin();
	float marginB = m_minkowskiB->GetMargin();

	bool isValid = false;
	bool checkSimplex = false;
	bool checkPenetration = true;

	{
		SimdScalar squaredDistance = SIMD_INFINITY;
		SimdScalar delta = 0.f;
		
		SimdScalar margin = marginA + marginB;
		
		

		m_simplexSolver->reset();
		
		while (true)
		{

			SimdVector3 seperatingAxisInA = (-m_cachedSeparatingAxis)* input.m_transformA.getBasis();
			SimdVector3 seperatingAxisInB = m_cachedSeparatingAxis* input.m_transformB.getBasis();

			SimdVector3 pInA = m_minkowskiA->LocalGetSupportingVertexWithoutMargin(seperatingAxisInA);
			SimdVector3 qInB = m_minkowskiB->LocalGetSupportingVertexWithoutMargin(seperatingAxisInB);
			SimdPoint3  pWorld = input.m_transformA(pInA);	
			SimdPoint3  qWorld = input.m_transformB(qInB);
			
			SimdVector3 w	= pWorld - qWorld;
			delta = m_cachedSeparatingAxis.dot(w);

			// potential exit, they don't overlap
			if ((delta > SimdScalar(0.0)) && (delta * delta > squaredDistance * input.m_maximumDistanceSquared)) 
			{
				checkPenetration = false;
				break;
			}

			//exit 0: the new point is already in the simplex, or we didn't come any closer
			if (m_simplexSolver->inSimplex(w))
			{
				checkSimplex = true;
				break;
			}
			// are we getting any closer ?
			if (squaredDistance - delta <= squaredDistance * rel_error2)
			{
				checkSimplex = true;
				break;
			}
			//add current vertex to simplex
			m_simplexSolver->addVertex(w, pWorld, qWorld);

			//calculate the closest point to the origin (update vector v)
			if (!m_simplexSolver->closest(m_cachedSeparatingAxis))
			{
				checkSimplex = true;
				break;
			}

			SimdScalar previousSquaredDistance = squaredDistance;
			squaredDistance = m_cachedSeparatingAxis.length2();
			
			//redundant m_simplexSolver->compute_points(pointOnA, pointOnB);

			//are we getting any closer ?
			if (previousSquaredDistance - squaredDistance <= SIMD_EPSILON * previousSquaredDistance) 
			{ 
				m_simplexSolver->backup_closest(m_cachedSeparatingAxis);
				checkSimplex = true;
				break;
			}
			bool check = (!m_simplexSolver->fullSimplex() && squaredDistance > SIMD_EPSILON * m_simplexSolver->maxVertex());

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
			float lenSqr = m_cachedSeparatingAxis.length2();
			//valid normal
			if (lenSqr > SIMD_EPSILON)
			{
				float rlen = 1.f / sqrtf(lenSqr );
				normalInB *= rlen; //normalize
				SimdScalar s = sqrtf(squaredDistance);
				ASSERT(s > SimdScalar(0.0));
				pointOnA -= m_cachedSeparatingAxis * (marginA / s);
				pointOnB += m_cachedSeparatingAxis * (marginB / s);
				distance = ((1.f/rlen) - margin);
				isValid = true;
			}
		}

		if (checkPenetration && !isValid)
		{
			//penetration case
			m_minkowskiA->SetMargin(marginA);
			m_minkowskiB->SetMargin(marginB);

			//if there is no way to handle penetrations, bail out
			if (m_penetrationDepthSolver)
			{
				// Penetration depth case.
				isValid = m_penetrationDepthSolver->CalcPenDepth( 
					*m_simplexSolver, 
					m_minkowskiA,m_minkowskiB,
					input.m_transformA,input.m_transformB,
					m_cachedSeparatingAxis, pointOnA, pointOnB);

				if (isValid)
				{
					normalInB = pointOnB-pointOnA;
					float lenSqr = normalInB.length2();
					if (lenSqr > SIMD_EPSILON)
					{
						normalInB /= sqrtf(lenSqr);
						distance = -(pointOnA-pointOnB).length();
					} else
					{
						isValid = false;
					}
				}
			}
		}
	}

	if (isValid)
	{
		output.AddContactPoint(
			normalInB,
			pointOnB,
			distance);
	} 


}





