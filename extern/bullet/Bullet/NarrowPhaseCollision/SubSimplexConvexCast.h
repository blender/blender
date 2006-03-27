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
