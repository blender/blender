/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
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

/* 3D BoundBox */

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

	enum empty_t { empty = 0};

	__forceinline BoundBox(empty_t)
	: min(make_float3(FLT_MAX, FLT_MAX, FLT_MAX)), max(make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX))
	{
	}

	__forceinline void grow(const float3& pt)  
	{
		/* the order of arguments to min is such that if pt is nan, it will not
		 * influence the resulting bounding box */
		min = ccl::min(pt, min);
		max = ccl::max(pt, max);
	}

	__forceinline void grow(const float3& pt, float border)  
	{
		float3 shift = make_float3(border, border, border);
		min = ccl::min(pt - shift, min);
		max = ccl::max(pt + shift, max);
	}

	__forceinline void grow(const BoundBox& bbox)
	{
		grow(bbox.min);
		grow(bbox.max);
	}

	__forceinline void grow_safe(const float3& pt)  
	{
		/* the order of arguments to min is such that if pt is nan, it will not
		 * influence the resulting bounding box */
		if(isfinite(pt.x) && isfinite(pt.y) && isfinite(pt.z)) {
			min = ccl::min(pt, min);
			max = ccl::max(pt, max);
		}
	}

	__forceinline void grow_safe(const float3& pt, float border)  
	{
		if(isfinite(pt.x) && isfinite(pt.y) && isfinite(pt.z) && isfinite(border)) {
			float3 shift = make_float3(border, border, border);
			min = ccl::min(pt - shift, min);
			max = ccl::max(pt + shift, max);
		}
	}

	__forceinline void grow_safe(const BoundBox& bbox)
	{
		grow_safe(bbox.min);
		grow_safe(bbox.max);
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

/* 2D BoundBox */

class BoundBox2D {
public:
	float left;
	float right;
	float bottom;
	float top;

	BoundBox2D()
	: left(0.0f), right(1.0f), bottom(0.0f), top(1.0f)
	{
	}

	bool operator==(const BoundBox2D& other) const
	{
		return (left == other.left && right == other.right &&
		        bottom == other.bottom && top == other.top);
	}

	float width()
	{
		return right - left;
	}

	float height()
	{
		return top - bottom;
	}

	BoundBox2D operator*(float f) const
	{
		BoundBox2D result;

		result.left = left*f;
		result.right = right*f;
		result.bottom = bottom*f;
		result.top = top*f;

		return result;
	}

	BoundBox2D subset(const BoundBox2D& other) const
	{
		BoundBox2D subset;

		subset.left = left + other.left*(right - left);
		subset.right = left + other.right*(right - left);
		subset.bottom = bottom + other.bottom*(top - bottom);
		subset.top = bottom + other.top*(top - bottom);

		return subset;
	}

	BoundBox2D make_relative_to(const BoundBox2D& other) const
	{
		BoundBox2D result;

		result.left = ((left - other.left) / (other.right - other.left));
		result.right = ((right - other.left) / (other.right - other.left));
		result.bottom = ((bottom - other.bottom) / (other.top - other.bottom));
		result.top = ((top - other.bottom) / (other.top - other.bottom));

		return result;
	}

	BoundBox2D clamp(float mn = 0.0f, float mx = 1.0f)
	{
		BoundBox2D result;

		result.left = ccl::clamp(left, mn, mx);
		result.right = ccl::clamp(right, mn, mx);
		result.bottom = ccl::clamp(bottom, mn, mx);
		result.top = ccl::clamp(top, mn, mx);

		return result;
	}
};

CCL_NAMESPACE_END

#endif /* __UTIL_BOUNDBOX_H__ */

