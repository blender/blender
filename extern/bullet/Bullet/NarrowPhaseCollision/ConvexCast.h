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
