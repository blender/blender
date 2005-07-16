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



#ifndef GJK_PAIR_DETECTOR_H
#define GJK_PAIR_DETECTOR_H

#include "DiscreteCollisionDetectorInterface.h"
#include "SimdPoint3.h"

#include "CollisionMargin.h"

class ConvexShape;
#include "SimplexSolverInterface.h"
class ConvexPenetrationDepthSolver;

/// GjkPairDetector uses GJK to implement the DiscreteCollisionDetectorInterface
class GjkPairDetector : public DiscreteCollisionDetectorInterface
{
	

	SimdVector3	m_cachedSeparatingAxis;
	ConvexPenetrationDepthSolver*	m_penetrationDepthSolver;
	SimplexSolverInterface* m_simplexSolver;
	ConvexShape* m_minkowskiA;
	ConvexShape* m_minkowskiB;

public:

	GjkPairDetector(ConvexShape* objectA,ConvexShape* objectB,SimplexSolverInterface* simplexSolver,ConvexPenetrationDepthSolver*	penetrationDepthSolver);
	virtual ~GjkPairDetector() {};

	virtual void	GetClosestPoints(const ClosestPointInput& input,Result& output);

	void SetMinkowskiA(ConvexShape* minkA)
	{
		m_minkowskiA = minkA;
	}

	void SetMinkowskiB(ConvexShape* minkB)
	{
		m_minkowskiB = minkB;
	}
	void SetCachedSeperatingAxis(const SimdVector3& seperatingAxis)
	{
		m_cachedSeparatingAxis = seperatingAxis;
	}

};

#endif //GJK_PAIR_DETECTOR_H
