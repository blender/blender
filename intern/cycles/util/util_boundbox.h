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
#include "util_transform.h"
#include "util_types.h"

using namespace std;

CCL_NAMESPACE_BEGIN

class BoundBox
{
public:
	float3 min, max;

	BoundBox(void)
	{
		min = make_float3(FLT_MAX, FLT_MAX, FLT_MAX);
		max = make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	}

	BoundBox(const float3& min_, const float3& max_)
	: min(min_), max(max_)
	{
	}

	void grow(const float3& pt)  
	{
		min = ccl::min(min, pt);
		max = ccl::max(max, pt);
	}

	void grow(const BoundBox& bbox)
	{
		grow(bbox.min);
		grow(bbox.max);
	}

	void intersect(const BoundBox& bbox) 
	{
		min = ccl::max(min, bbox.min);
		max = ccl::min(max, bbox.max);
	}

	float area(void) const
	{
		if(!valid())
			return 0.0f;

		float3 d = max - min;
		return dot(d, d)*2.0f;
	}

	bool valid(void) const
	{
		return (min.x <= max.x) && (min.y <= max.y) && (min.z <= max.z) &&
		       (isfinite(min.x) && isfinite(min.y) && isfinite(min.z)) &&
		       (isfinite(max.x) && isfinite(max.y) && isfinite(max.z));
	}

	BoundBox transformed(const Transform *tfm)
	{
		BoundBox result;

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

CCL_NAMESPACE_END

#endif /* __UTIL_BOUNDBOX_H__ */

