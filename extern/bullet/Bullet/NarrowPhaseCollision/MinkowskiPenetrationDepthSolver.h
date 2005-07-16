
#ifndef MINKOWSKI_PENETRATION_DEPTH_SOLVER_H
#define MINKOWSKI_PENETRATION_DEPTH_SOLVER_H

#include "ConvexPenetrationDepthSolver.h"

///MinkowskiPenetrationDepthSolver implements bruteforce penetration depth estimation.
///Implementation is based on sampling the depth using support mapping, and using GJK step to get the witness points.
class MinkowskiPenetrationDepthSolver : public ConvexPenetrationDepthSolver
{
public:

	virtual bool CalcPenDepth( SimplexSolverInterface& simplexSolver,
	ConvexShape* convexA,ConvexShape* convexB,
				const SimdTransform& transA,const SimdTransform& transB,
			SimdVector3& v, SimdPoint3& pa, SimdPoint3& pb);

};

#endif //MINKOWSKI_PENETRATION_DEPTH_SOLVER_H