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
		max_leaf_size = 0x7FFFFFF;

		top_level = false;
		use_cache = false;
		use_qbvh = false;
		pad = false;
	}

	/* SAH costs */
	float cost(int num_nodes, int num_tris) const
	{ return node_cost(num_nodes) + triangle_cost(num_tris); }

	float triangle_cost(int n) const
	{ return n*sah_triangle_cost; }

	float node_cost(int n) const
	{ return n*sah_node_cost; }
};

CCL_NAMESPACE_END

#endif /* __BVH_PARAMS_H__ */

