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

#ifndef __BVH_H__
#define __BVH_H__

#include "bvh/bvh_params.h"
#include "util/util_array.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Stats;
class Device;
class DeviceScene;
class BVHNode;
struct BVHStackEntry;
class BVHParams;
class BoundBox;
class LeafNode;
class Geometry;
class Object;
class Progress;

#define BVH_ALIGN 4096
#define TRI_NODE_SIZE 3
/* Packed BVH
 *
 * BVH stored as it will be used for traversal on the rendering device. */

struct PackedBVH {
  /* BVH nodes storage, one node is 4x int4, and contains two bounding boxes,
   * and child, triangle or object indexes depending on the node type */
  array<int4> nodes;
  /* BVH leaf nodes storage. */
  array<int4> leaf_nodes;
  /* object index to BVH node index mapping for instances */
  array<int> object_node;
  /* Mapping from primitive index to index in triangle array. */
  array<uint> prim_tri_index;
  /* Continuous storage of triangle vertices. */
  array<float4> prim_tri_verts;
  /* primitive type - triangle or strand */
  array<int> prim_type;
  /* visibility visibilitys for primitives */
  array<uint> prim_visibility;
  /* mapping from BVH primitive index to true primitive index, as primitives
   * may be duplicated due to spatial splits. -1 for instances. */
  array<int> prim_index;
  /* mapping from BVH primitive index, to the object id of that primitive. */
  array<int> prim_object;
  /* Time range of BVH primitive. */
  array<float2> prim_time;

  /* index of the root node. */
  int root_index;

  PackedBVH()
  {
    root_index = 0;
  }
};

enum BVH_TYPE { bvh2 };

/* BVH */

class BVH {
 public:
  PackedBVH pack;
  BVHParams params;
  vector<Geometry *> geometry;
  vector<Object *> objects;

  static BVH *create(const BVHParams &params,
                     const vector<Geometry *> &geometry,
                     const vector<Object *> &objects);
  virtual ~BVH()
  {
  }

  virtual void build(Progress &progress, Stats *stats = NULL);
  virtual void copy_to_device(Progress & /*progress*/, DeviceScene * /*dscene*/)
  {
  }

  void refit(Progress &progress);

 protected:
  BVH(const BVHParams &params,
      const vector<Geometry *> &geometry,
      const vector<Object *> &objects);

  /* Refit range of primitives. */
  void refit_primitives(int start, int end, BoundBox &bbox, uint &visibility);

  /* triangles and strands */
  void pack_primitives();
  void pack_triangle(int idx, float4 storage[3]);

  /* merge instance BVH's */
  void pack_instances(size_t nodes_size, size_t leaf_nodes_size);

  /* for subclasses to implement */
  virtual void pack_nodes(const BVHNode *root) = 0;
  virtual void refit_nodes() = 0;

  virtual BVHNode *widen_children_nodes(const BVHNode *root) = 0;
};

/* Pack Utility */
struct BVHStackEntry {
  const BVHNode *node;
  int idx;

  BVHStackEntry(const BVHNode *n = 0, int i = 0);
  int encodeIdx() const;
};

CCL_NAMESPACE_END

#endif /* __BVH_H__ */
