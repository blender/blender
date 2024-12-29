/* SPDX-FileCopyrightText: 2009-2010 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#pragma once

#include "bvh/bvh.h"
#include "bvh/params.h"

#include "util/types.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

// NOLINTBEGIN
#define BVH_NODE_SIZE 4
#define BVH_NODE_LEAF_SIZE 1
#define BVH_UNALIGNED_NODE_SIZE 7
// NOLINTEND

/* Pack Utility */
struct BVHStackEntry {
  const BVHNode *node;
  int idx;

  BVHStackEntry(const BVHNode *n = nullptr, const int i = 0);
  int encodeIdx() const;
};

/* BVH2
 *
 * Typical BVH with each node having two children.
 */
class BVH2 : public BVH {
 public:
  BVH2(const BVHParams &params,
       const vector<Geometry *> &geometry,
       const vector<Object *> &objects);

  void build(Progress &progress, Stats *stats);
  void refit(Progress &progress);

  PackedBVH pack;

 protected:
  /* Building process. */
  virtual unique_ptr<BVHNode> widen_children_nodes(unique_ptr<BVHNode> &&root);

  /* pack */
  void pack_nodes(const BVHNode *root);

  void pack_leaf(const BVHStackEntry &e, const LeafNode *leaf);
  void pack_inner(const BVHStackEntry &e, const BVHStackEntry &e0, const BVHStackEntry &e1);

  void pack_aligned_inner(const BVHStackEntry &e,
                          const BVHStackEntry &e0,
                          const BVHStackEntry &e1);
  void pack_aligned_node(const int idx,
                         const BoundBox &b0,
                         const BoundBox &b1,
                         int c0,
                         int c1,
                         uint visibility0,
                         uint visibility1);

  void pack_unaligned_inner(const BVHStackEntry &e,
                            const BVHStackEntry &e0,
                            const BVHStackEntry &e1);
  void pack_unaligned_node(const int idx,
                           const Transform &aligned_space0,
                           const Transform &aligned_space1,
                           const BoundBox &b0,
                           const BoundBox &b1,
                           int c0,
                           int c1,
                           uint visibility0,
                           uint visibility1);

  /* refit */
  void refit_nodes();
  void refit_node(const int idx, bool leaf, BoundBox &bbox, uint &visibility);

  /* Refit range of primitives. */
  void refit_primitives(const int start, const int end, BoundBox &bbox, uint &visibility);

  /* triangles and strands */
  void pack_primitives();
  void pack_triangle(const int idx, const float4 storage[3]);

  /* merge instance BVH's */
  void pack_instances(const size_t nodes_size, const size_t leaf_nodes_size);
};

CCL_NAMESPACE_END
