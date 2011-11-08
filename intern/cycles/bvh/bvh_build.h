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

#ifndef __BVH_BUILD_H__
#define __BVH_BUILD_H__

#include <float.h>

#include "bvh.h"

#include "util_boundbox.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class BVHParams;
class Mesh;
class Object;
class Progress;

/* BVH Builder */

class BVHBuild
{
public:
	struct Reference
	{
		int prim_index;
		int prim_object;
		BoundBox bounds;

		Reference()
		{
		}
	};

	struct NodeSpec
	{
		int num;
		BoundBox bounds;

		NodeSpec()
		{
			num = 0;
		}
	};

	BVHBuild(
		const vector<Object*>& objects,
		vector<int>& prim_index,
		vector<int>& prim_object,
		const BVHParams& params,
		Progress& progress);
	~BVHBuild();

	BVHNode *run();

protected:
	/* adding references */
	void add_reference_mesh(NodeSpec& root, Mesh *mesh, int i);
	void add_reference_object(NodeSpec& root, Object *ob, int i);
	void add_references(NodeSpec& root);

	/* building */
	BVHNode *build_node(const NodeSpec& spec, int level, float progress_start, float progress_end);
	BVHNode *create_leaf_node(const NodeSpec& spec);
	BVHNode *create_object_leaf_nodes(const Reference *ref, int num);

	void progress_update(float progress_start, float progress_end);

	/* object splits */
	struct ObjectSplit
	{
		float sah;
		int dim;
		int num_left;
		BoundBox left_bounds;
		BoundBox right_bounds;

		ObjectSplit()
		: sah(FLT_MAX), dim(0), num_left(0)
		{
		}
	};

	ObjectSplit find_object_split(const NodeSpec& spec, float nodeSAH);
	void do_object_split(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const ObjectSplit& split);

	/* spatial splits */
	struct SpatialSplit
	{
		float sah;
		int dim;
		float pos;

		SpatialSplit()
		: sah(FLT_MAX), dim(0), pos(0.0f)
		{
		}
	};

	struct SpatialBin
	{
		BoundBox bounds;
		int enter;
		int exit;
	};

	SpatialSplit find_spatial_split(const NodeSpec& spec, float nodeSAH);
	void do_spatial_split(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const SpatialSplit& split);
	void split_reference(Reference& left, Reference& right, const Reference& ref, int dim, float pos);

	/* objects and primitive references */
	vector<Object*> objects;
	vector<Reference> references;

	/* output primitive indexes and objects */
	vector<int>& prim_index;
	vector<int>& prim_object;

	/* build parameters */
	BVHParams params;

	/* progress reporting */
	Progress& progress;
	double progress_start_time;
	int progress_num_duplicates;

	/* spatial splitting */
	float spatial_min_overlap;
	vector<BoundBox> spatial_right_bounds;
	SpatialBin spatial_bins[3][BVHParams::NUM_SPATIAL_BINS];
};

CCL_NAMESPACE_END

#endif /* __BVH_BUILD_H__ */

