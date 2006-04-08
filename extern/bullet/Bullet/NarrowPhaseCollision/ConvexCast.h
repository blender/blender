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


#ifndef CONVEX_CAST_H
#define CONVEX_CAST_H

#include <SimdTransform.h>
#include <SimdVector3.h>
#include <SimdScalar.h>
class MinkowskiSumShape;
#include "IDebugDraw.h"

/// ConvexCast is an interface for Casting
class ConvexCast
{
public:


	virtual ~ConvexCast();

	///RayResult stores the closest result
	/// alternatively, add a callback method to decide about closest/all results
	struct	CastResult
	{
		//virtual bool	addRayResult(const SimdVector3& normal,SimdScalar	fraction) = 0;
				
		virtual void	DebugDraw(SimdScalar	fraction) {}
		virtual void	DrawCoordSystem(const SimdTransform& trans) {}

		CastResult()
			:m_fraction(1e30f),
			m_debugDrawer(0)
		{
		}


		virtual ~CastResult() {};

		SimdVector3	m_normal;
		SimdScalar	m_fraction;
		SimdTransform	m_hitTransformA;
		SimdTransform	m_hitTransformB;

		IDebugDraw* m_debugDrawer;

	};


	/// cast a convex against another convex object
	virtual bool	calcTimeOfImpact(
					const SimdTransform& fromA,
					const SimdTransform& toA,
					const SimdTransform& fromB,
					const SimdTransform& toB,
					CastResult& result) = 0;
};

#endif //CONVEX_CAST_H
