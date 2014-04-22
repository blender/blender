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

#ifndef __BVH_SPLIT_H__
#define __BVH_SPLIT_H__

#include "bvh_build.h"
#include "bvh_params.h"

CCL_NAMESPACE_BEGIN

class BVHBuild;

/* Object Split */

class BVHObjectSplit
{
public:
	float sah;
	int dim;
	int num_left;
	BoundBox left_bounds;
	BoundBox right_bounds;

	BVHObjectSplit() {}
	BVHObjectSplit(BVHBuild *builder, const BVHRange& range, float nodeSAH);

	void split(BVHBuild *builder, BVHRange& left, BVHRange& right, const BVHRange& range);
};

/* Spatial Split */

class BVHSpatialSplit
{
public:
	float sah;
	int dim;
	float pos;

	BVHSpatialSplit() : sah(FLT_MAX), dim(0), pos(0.0f) {}
	BVHSpatialSplit(BVHBuild *builder, const BVHRange& range, float nodeSAH);

	void split(BVHBuild *builder, BVHRange& left, BVHRange& right, const BVHRange& range);
	void split_reference(BVHBuild *builder, BVHReference& left, BVHReference& right, const BVHReference& ref, int dim, float pos);
};

/* Mixed Object-Spatial Split */

class BVHMixedSplit
{
public:
	BVHObjectSplit object;
	BVHSpatialSplit spatial;

	float leafSAH;
	float nodeSAH;
	float minSAH;

	bool no_split;

	__forceinline BVHMixedSplit(BVHBuild *builder, const BVHRange& range, int level)
	{
		/* find split candidates. */
		float area = range.bounds().safe_area();

		leafSAH = area * builder->params.primitive_cost(range.size());
		nodeSAH = area * builder->params.node_cost(2);

		object = BVHObjectSplit(builder, range, nodeSAH);

		if(builder->params.use_spatial_split && level < BVHParams::MAX_SPATIAL_DEPTH) {
			BoundBox overlap = object.left_bounds;
			overlap.intersect(object.right_bounds);

			if(overlap.safe_area() >= builder->spatial_min_overlap)
				spatial = BVHSpatialSplit(builder, range, nodeSAH);
		}

		/* leaf SAH is the lowest => create leaf. */
		minSAH = min(min(leafSAH, object.sah), spatial.sah);
		no_split = (minSAH == leafSAH && builder->range_within_max_leaf_size(range));
	}

	__forceinline void split(BVHBuild *builder, BVHRange& left, BVHRange& right, const BVHRange& range)
	{
		if(builder->params.use_spatial_split && minSAH == spatial.sah)
			spatial.split(builder, left, right, range);
		if(!left.size() || !right.size())
			object.split(builder, left, right, range);
	}
};

CCL_NAMESPACE_END

#endif /* __BVH_SPLIT_H__ */

