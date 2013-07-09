/*
 * Adapted from code copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
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
 * limitations under the License.
 */

#ifndef __BVH_PARAMS_H__
#define __BVH_PARAMS_H__

#include "util_boundbox.h"

CCL_NAMESPACE_BEGIN

/* BVH Parameters */

class BVHParams
{
public:
	/* spatial split area threshold */
	int use_spatial_split;
	float spatial_split_alpha;

	/* SAH costs */
	float sah_node_cost;
	float sah_triangle_cost;

	/* number of triangles in leaf */
	int min_leaf_size;
	int max_leaf_size;

	/* object or mesh level bvh */
	int top_level;

	/* disk cache */
	int use_cache;

	/* QBVH */
	int use_qbvh;

	int pad;

	/* fixed parameters */
	enum {
		MAX_DEPTH = 64,
		MAX_SPATIAL_DEPTH = 48,
		NUM_SPATIAL_BINS = 32
	};

	BVHParams()
	{
		use_spatial_split = true;
		spatial_split_alpha = 1e-5f;

		sah_node_cost = 1.0f;
		sah_triangle_cost = 1.0f;

		min_leaf_size = 1;
		max_leaf_size = 8;

		top_level = false;
		use_cache = false;
		use_qbvh = false;
		pad = false;
	}

	/* SAH costs */
	__forceinline float cost(int num_nodes, int num_tris) const
	{ return node_cost(num_nodes) + triangle_cost(num_tris); }

	__forceinline float triangle_cost(int n) const
	{ return n*sah_triangle_cost; }

	__forceinline float node_cost(int n) const
	{ return n*sah_node_cost; }

	__forceinline bool small_enough_for_leaf(int size, int level)
	{ return (size <= min_leaf_size || level >= MAX_DEPTH); }
};

/* BVH Reference
 *
 * Reference to a primitive. Primitive index and object are sneakily packed
 * into BoundBox to reduce memory usage and align nicely */

class BVHReference
{
public:
	__forceinline BVHReference() {}

	__forceinline BVHReference(const BoundBox& bounds_, int prim_index_, int prim_object_, int prim_segment)
	: rbounds(bounds_)
	{
		rbounds.min.w = __int_as_float(prim_index_);
		rbounds.max.w = __int_as_float(prim_object_);
		segment = prim_segment;
	}

	__forceinline const BoundBox& bounds() const { return rbounds; }
	__forceinline int prim_index() const { return __float_as_int(rbounds.min.w); }
	__forceinline int prim_object() const { return __float_as_int(rbounds.max.w); }
	__forceinline int prim_segment() const { return segment; }

protected:
	BoundBox rbounds;
	uint segment;
};

/* BVH Range
 *
 * Build range used during construction, to indicate the bounds and place in
 * the reference array of a subset of pirmitives Again uses trickery to pack
 * integers into BoundBox for alignment purposes. */

class BVHRange
{
public:
	__forceinline BVHRange()
	{
		rbounds.min.w = __int_as_float(0);
		rbounds.max.w = __int_as_float(0);
	}

	__forceinline BVHRange(const BoundBox& bounds_, int start_, int size_)
	: rbounds(bounds_)
	{
		rbounds.min.w = __int_as_float(start_);
		rbounds.max.w = __int_as_float(size_);
	}

	__forceinline BVHRange(const BoundBox& bounds_, const BoundBox& cbounds_, int start_, int size_)
	: rbounds(bounds_), cbounds(cbounds_)
	{
		rbounds.min.w = __int_as_float(start_);
		rbounds.max.w = __int_as_float(size_);
	}

	__forceinline void set_start(int start_) { rbounds.min.w = __int_as_float(start_); }

	__forceinline const BoundBox& bounds() const { return rbounds; }
	__forceinline const BoundBox& cent_bounds() const { return cbounds; }
	__forceinline int start() const { return __float_as_int(rbounds.min.w); }
	__forceinline int size() const { return __float_as_int(rbounds.max.w); }
	__forceinline int end() const { return start() + size(); }

protected:
	BoundBox rbounds;
	BoundBox cbounds;
};

/* BVH Spatial Bin */

struct BVHSpatialBin
{
	BoundBox bounds;
	int enter;
	int exit;

	__forceinline BVHSpatialBin()
	{
	}
};

CCL_NAMESPACE_END

#endif /* __BVH_PARAMS_H__ */

