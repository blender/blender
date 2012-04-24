/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __UTIL_BOUNDBOX_H__
#define __UTIL_BOUNDBOX_H__

#include <math.h>
#include <float.h>

#include "util_math.h"
#include "util_string.h"
#include "util_transform.h"
#include "util_types.h"

using namespace std;

CCL_NAMESPACE_BEGIN

class BoundBox
{
public:
	float3 min, max;

	__forceinline BoundBox()
	{
	}

	__forceinline BoundBox(const float3& pt)
	: min(pt), max(pt)
	{
	}

	__forceinline BoundBox(const float3& min_, const float3& max_)
	: min(min_), max(max_)
	{
	}

	static struct empty_t {} empty;

	__forceinline BoundBox(empty_t)
	: min(make_float3(FLT_MAX, FLT_MAX, FLT_MAX)), max(make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX))
	{
	}

	__forceinline void grow(const float3& pt)  
	{
		min = ccl::min(min, pt);
		max = ccl::max(max, pt);
	}

	__forceinline void grow(const BoundBox& bbox)
	{
		grow(bbox.min);
		grow(bbox.max);
	}

	__forceinline void intersect(const BoundBox& bbox) 
	{
		min = ccl::max(min, bbox.min);
		max = ccl::min(max, bbox.max);
	}

	/* todo: avoid using this */
	__forceinline float safe_area() const
	{
		if(!((min.x <= max.x) && (min.y <= max.y) && (min.z <= max.z)))
			return 0.0f;

		return area();
	}

	__forceinline float area() const
	{
		return half_area()*2.0f;
	}

	__forceinline float half_area() const
	{
		float3 d = max - min;
		return (d.x*d.z + d.y*d.z + d.x*d.y);
	}

	__forceinline float3 center() const
	{
		return 0.5f*(min + max);
	}

	__forceinline float3 center2() const
	{
		return min + max;
	}

	__forceinline float3 size() const
	{
		return max - min;
	}
	
	__forceinline bool valid() const
	{
		return (min.x <= max.x) && (min.y <= max.y) && (min.z <= max.z) &&
		       (isfinite(min.x) && isfinite(min.y) && isfinite(min.z)) &&
		       (isfinite(max.x) && isfinite(max.y) && isfinite(max.z));
	}

	BoundBox transformed(const Transform *tfm)
	{
		BoundBox result = BoundBox::empty;

		for(int i = 0; i < 8; i++) {
			float3 p;

			p.x = (i & 1)? min.x: max.x;
			p.y = (i & 2)? min.y: max.y;
			p.z = (i & 4)? min.z: max.z;

			result.grow(transform_point(tfm, p));
		}

		return result;
	}
};

__forceinline BoundBox merge(const BoundBox& bbox, const float3& pt)
{
	return BoundBox(min(bbox.min, pt), max(bbox.max, pt));
}

__forceinline BoundBox merge(const BoundBox& a, const BoundBox& b)
{
	return BoundBox(min(a.min, b.min), max(a.max, b.max));
}

__forceinline BoundBox merge(const BoundBox& a, const BoundBox& b, const BoundBox& c, const BoundBox& d)
{
	return merge(merge(a, b), merge(c, d));
}

__forceinline BoundBox intersect(const BoundBox& a, const BoundBox& b)
{
	return BoundBox(max(a.min, b.min), min(a.max, b.max));
}

__forceinline BoundBox intersect(const BoundBox& a, const BoundBox& b, const BoundBox& c)
{
	return intersect(a, intersect(b, c));
}

CCL_NAMESPACE_END

#endif /* __UTIL_BOUNDBOX_H__ */

