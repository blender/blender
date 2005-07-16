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

#ifndef CONVEX_PENETRATION_DEPTH_H
#define CONVEX_PENETRATION_DEPTH_H

class SimdVector3;
#include "SimplexSolverInterface.h"
class ConvexShape;
#include "SimdPoint3.h"
class SimdTransform;

///ConvexPenetrationDepthSolver provides an interface for penetration depth calculation.
class ConvexPenetrationDepthSolver
{
public:	

	virtual bool CalcPenDepth( SimplexSolverInterface& simplexSolver,
		ConvexShape* convexA,ConvexShape* convexB,
					const SimdTransform& transA,const SimdTransform& transB,
				SimdVector3& v, SimdPoint3& pa, SimdPoint3& pb) = 0;


};
#endif //CONVEX_PENETRATION_DEPTH_H