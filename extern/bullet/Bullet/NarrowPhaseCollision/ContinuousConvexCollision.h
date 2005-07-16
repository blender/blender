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

#ifndef CONTINUOUS_COLLISION_CONVEX_CAST_H
#define CONTINUOUS_COLLISION_CONVEX_CAST_H

#include "ConvexCast.h"
#include "SimplexSolverInterface.h"
class ConvexPenetrationDepthSolver;
class ConvexShape;

/// ContinuousConvexCollision implements angular and linear time of impact for convex objects.
/// Based on Brian Mirtich's Conservative Advancement idea (PhD thesis).
/// Algorithm operates in worldspace, in order to keep inbetween motion globally consistent.
/// It uses GJK at the moment. Future improvement would use minkowski sum / supporting vertex, merging innerloops
class ContinuousConvexCollision : public ConvexCast
{
	SimplexSolverInterface* m_simplexSolver;
	ConvexPenetrationDepthSolver*	m_penetrationDepthSolver;
	ConvexShape*	m_convexA;
	ConvexShape*	m_convexB;


public:

	ContinuousConvexCollision (ConvexShape*	shapeA,ConvexShape*	shapeB ,SimplexSolverInterface* simplexSolver,ConvexPenetrationDepthSolver* penetrationDepthSolver);

	virtual bool	calcTimeOfImpact(
				const SimdTransform& fromA,
				const SimdTransform& toA,
				const SimdTransform& fromB,
				const SimdTransform& toB,
				CastResult& result);


};

#endif //CONTINUOUS_COLLISION_CONVEX_CAST_H