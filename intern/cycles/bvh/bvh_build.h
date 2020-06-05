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

#include "bvh/bvh_params.h"
#include "bvh/bvh_unaligned.h"

#include "util/util_array.h"
#include "util/util_task.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Boundbox;
class BVHBuildTask;
class BVHNode;
class BVHSpatialSplitBuildTask;
class BVHParams;
class InnerNode;
class Geometry;
class Hair;
class Mesh;
class Object;
class Progress;

/* BVH Builder */

class BVHBuild {
 public:
  /* Constructor/Destructor */
  BVHBuild(const vector<Object *> &objects,
           array<int> &prim_type,
           array<int> &prim_index,
           array<int> &prim_object,
           array<float2> &prim_time,
           const BVHParams &params,
           Progress &progress);
  ~BVHBuild();

  BVHNode *run();

 protected:
  friend class BVHMixedSplit;
  friend class BVHObjectSplit;
  friend class BVHSpatialSplit;
  friend class BVHBuildTask;
  friend class BVHSpatialSplitBuildTask;
  friend class BVHObjectBinning;

  /* Adding references. */
  void add_reference_triangles(BoundBox &root, BoundBox &center, Mesh *mesh, int i);
  void add_reference_curves(BoundBox &root, BoundBox &center, Hair *hair, int i);
  void add_reference_geometry(BoundBox &root, BoundBox &center, Geometry *geom, int i);
  void add_reference_object(BoundBox &root, BoundBox &center, Object *ob, int i);
  void add_references(BVHRange &root);

  /* Building. */
  BVHNode *build_node(const BVHRange &range,
                      vector<BVHReference> *references,
                      int level,
                      BVHSpatialStorage *storage);
  BVHNode *build_node(const BVHObjectBinning &range, int level);
  BVHNode *create_leaf_node(const BVHRange &range, const vector<BVHReference> &references);
  BVHNode *create_object_leaf_nodes(const BVHReference *ref, int start, int num);

  bool range_within_max_leaf_size(const BVHRange &range,
                                  const vector<BVHReference> &references) const;

  /* Threads. */
  enum { THREAD_TASK_SIZE = 4096 };
  void thread_build_node(InnerNode *node, int child, BVHObjectBinning *range, int level);
  void thread_build_spatial_split_node(
      InnerNode *node, int child, BVHRange *range, vector<BVHReference> *references, int level);
  thread_mutex build_mutex;

  /* Progress. */
  void progress_update();

  /* Tree rotations. */
  void rotate(BVHNode *node, int max_depth);
  void rotate(BVHNode *node, int max_depth, int iterations);

  /* Objects and primitive references. */
  vector<Object *> objects;
  vector<BVHReference> references;
  int num_original_references;

  /* Output primitive indexes and objects. */
  array<int> &prim_type;
  array<int> &prim_index;
  array<int> &prim_object;
  array<float2> &prim_time;

  bool need_prim_time;

  /* Build parameters. */
  BVHParams params;

  /* Progress reporting. */
  Progress &progress;
  double progress_start_time;
  size_t progress_count;
  size_t progress_total;
  size_t progress_original_total;

  /* Spatial splitting. */
  float spatial_min_overlap;
  enumerable_thread_specific<BVHSpatialStorage> spatial_storage;
  size_t spatial_free_index;
  thread_spin_lock spatial_spin_lock;

  /* Threads. */
  TaskPool task_pool;

  /* Unaligned building. */
  BVHUnaligned unaligned_heuristic;
};

CCL_NAMESPACE_END

#endif /* __BVH_BUILD_H__ */
