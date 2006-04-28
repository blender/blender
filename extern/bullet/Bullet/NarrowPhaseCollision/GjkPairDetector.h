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




#ifndef GJK_PAIR_DETECTOR_H
#define GJK_PAIR_DETECTOR_H

#include "DiscreteCollisionDetectorInterface.h"
#include "SimdPoint3.h"

#include <CollisionShapes/CollisionMargin.h>

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
	bool		m_ignoreMargin;

public:

	GjkPairDetector(ConvexShape* objectA,ConvexShape* objectB,SimplexSolverInterface* simplexSolver,ConvexPenetrationDepthSolver*	penetrationDepthSolver);
	virtual ~GjkPairDetector() {};

	virtual void	GetClosestPoints(const ClosestPointInput& input,Result& output,class IDebugDraw* debugDraw);

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

	void	SetPenetrationDepthSolver(ConvexPenetrationDepthSolver*	penetrationDepthSolver)
	{
		m_penetrationDepthSolver = penetrationDepthSolver;
	}

	void	SetIgnoreMargin(bool ignoreMargin)
	{
		m_ignoreMargin = ignoreMargin;
	}

};

#endif //GJK_PAIR_DETECTOR_H
