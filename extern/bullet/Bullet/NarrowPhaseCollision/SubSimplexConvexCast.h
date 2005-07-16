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

#ifndef SUBSIMPLEX_CONVEX_CAST_H
#define SUBSIMPLEX_CONVEX_CAST_H

#include "ConvexCast.h"
#include "SimplexSolverInterface.h"
class ConvexShape;

/// SubsimplexConvexCast implements Gino van den Bergens' paper
/// GJK based Ray Cast, optimized version
class SubsimplexConvexCast : public ConvexCast
{
	SimplexSolverInterface* m_simplexSolver;
	ConvexShape*	m_convexA;
	ConvexShape*	m_convexB;

public:

	SubsimplexConvexCast (ConvexShape*	shapeA,ConvexShape*	shapeB,SimplexSolverInterface* simplexSolver);

	//virtual ~SubsimplexConvexCast();
	
	virtual bool	calcTimeOfImpact(
			const SimdTransform& fromA,
			const SimdTransform& toA,
			const SimdTransform& fromB,
			const SimdTransform& toB,
			CastResult& result);

};

#endif //SUBSIMPLEX_CONVEX_CAST_H
