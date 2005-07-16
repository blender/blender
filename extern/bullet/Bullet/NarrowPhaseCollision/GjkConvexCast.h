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


#ifndef GJK_CONVEX_CAST_H
#define GJK_CONVEX_CAST_H

#include "CollisionMargin.h"
#include "SimdVector3.h"
#include "ConvexCast.h"
class ConvexShape;
class MinkowskiSumShape;
#include "SimplexSolverInterface.h"

///GjkConvexCast performs a raycast on a convex object using support mapping.
class GjkConvexCast : public ConvexCast
{
	SimplexSolverInterface*	m_simplexSolver;
	ConvexShape*	m_convexA;
	ConvexShape*	m_convexB;

public:

	GjkConvexCast(ConvexShape*	convexA,ConvexShape* convexB,SimplexSolverInterface* simplexSolver);

	/// cast a convex against another convex object
	virtual bool	calcTimeOfImpact(
					const SimdTransform& fromA,
					const SimdTransform& toA,
					const SimdTransform& fromB,
					const SimdTransform& toB,
					CastResult& result);

};

#endif //GJK_CONVEX_CAST_H
