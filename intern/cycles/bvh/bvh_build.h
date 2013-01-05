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
#include "bvh_binning.h"

#include "util_boundbox.h"
#include "util_task.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class BVHBuildTask;
class BVHParams;
class InnerNode;
class Mesh;
class Object;
class Progress;

/* BVH Builder */

class BVHBuild
{
public:
	/* Constructor/Destructor */
	BVHBuild(
		const vector<Object*>& objects,
		vector<int>& prim_segment,
		vector<int>& prim_index,
		vector<int>& prim_object,
		const BVHParams& params,
		Progress& progress);
	~BVHBuild();

	BVHNode *run();

protected:
	friend class BVHMixedSplit;
	friend class BVHObjectSplit;
	friend class BVHSpatialSplit;
	friend class BVHBuildTask;

	/* adding references */
	void add_reference_mesh(BoundBox& root, BoundBox& center, Mesh *mesh, int i);
	void add_reference_object(BoundBox& root, BoundBox& center, Object *ob, int i);
	void add_references(BVHRange& root);

	/* building */
	BVHNode *build_node(const BVHRange& range, int level);
	BVHNode *build_node(const BVHObjectBinning& range, int level);
	BVHNode *create_leaf_node(const BVHRange& range);
	BVHNode *create_object_leaf_nodes(const BVHReference *ref, int start, int num);

	/* threads */
	enum { THREAD_TASK_SIZE = 4096 };
	void thread_build_node(InnerNode *node, int child, BVHObjectBinning *range, int level);
	thread_mutex build_mutex;

	/* progress */
	void progress_update();

	/* tree rotations */
	void rotate(BVHNode *node, int max_depth);
	void rotate(BVHNode *node, int max_depth, int iterations);

	/* objects and primitive references */
	vector<Object*> objects;
	vector<BVHReference> references;
	int num_original_references;

	/* output primitive indexes and objects */
	vector<int>& prim_segment;
	vector<int>& prim_index;
	vector<int>& prim_object;

	/* build parameters */
	BVHParams params;

	/* progress reporting */
	Progress& progress;
	double progress_start_time;
	size_t progress_count;
	size_t progress_total;
	size_t progress_original_total;

	/* spatial splitting */
	float spatial_min_overlap;
	vector<BoundBox> spatial_right_bounds;
	BVHSpatialBin spatial_bins[3][BVHParams::NUM_SPATIAL_BINS];

	/* threads */
	TaskPool task_pool;
};

CCL_NAMESPACE_END

#endif /* __BVH_BUILD_H__ */

