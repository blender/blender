/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#ifndef AABB_UTIL2
#define AABB_UTIL2

#include "SimdVector3.h"

#define SimdMin(a,b) ((a < b ? a : b))
#define SimdMax(a,b) ((a > b ? a : b))


/// conservative test for overlap between two aabbs
SIMD_FORCE_INLINE bool TestAabbAgainstAabb2(const SimdVector3 &aabbMin1, const SimdVector3 &aabbMax1,
								const SimdVector3 &aabbMin2, const SimdVector3 &aabbMax2)
{
	bool overlap = true;
	overlap = (aabbMin1[0] > aabbMax2[0] || aabbMax1[0] < aabbMin2[0]) ? false : overlap;
	overlap = (aabbMin1[2] > aabbMax2[2] || aabbMax1[2] < aabbMin2[2]) ? false : overlap;
	overlap = (aabbMin1[1] > aabbMax2[1] || aabbMax1[1] < aabbMin2[1]) ? false : overlap;
	return overlap;
}

/// conservative test for overlap between triangle and aabb
SIMD_FORCE_INLINE bool TestTriangleAgainstAabb2(const SimdVector3 *vertices,
									const SimdVector3 &aabbMin, const SimdVector3 &aabbMax)
{
	const SimdVector3 &p1 = vertices[0];
	const SimdVector3 &p2 = vertices[1];
	const SimdVector3 &p3 = vertices[2];

	if (SimdMin(SimdMin(p1[0], p2[0]), p3[0]) > aabbMax[0]) return false;
	if (SimdMax(SimdMax(p1[0], p2[0]), p3[0]) < aabbMin[0]) return false;

	if (SimdMin(SimdMin(p1[2], p2[2]), p3[2]) > aabbMax[2]) return false;
	if (SimdMax(SimdMax(p1[2], p2[2]), p3[2]) < aabbMin[2]) return false;
  
	if (SimdMin(SimdMin(p1[1], p2[1]), p3[1]) > aabbMax[1]) return false;
	if (SimdMax(SimdMax(p1[1], p2[1]), p3[1]) < aabbMin[1]) return false;
	return true;
}

#endif

