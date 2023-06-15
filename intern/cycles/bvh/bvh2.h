/* SPDX-FileCopyrightText: 2009-2010 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#ifndef __BVH2_H__
#define __BVH2_H__

#include "bvh/bvh.h"
#include "bvh/params.h"

#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

#define BVH_NODE_SIZE 4
#define BVH_NODE_LEAF_SIZE 1
#define BVH_UNALIGNED_NODE_SIZE 7

/* Pack Utility */
struct BVHStackEntry {
  const BVHNode *node;
  int idx;

  BVHStackEntry(const BVHNode *n = 0, int i = 0);
  int encodeIdx() const;
};

/* BVH2
 *
 * Typical BVH with each node having two children.
 */
class BVH2 : public BVH {
 public:
  void build(Progress &progress, Stats *stats);
  void refit(Progress &progress);

  PackedBVH pack;

 protected:
  /* constructor */
  friend class BVH;
  BVH2(const BVHParams &params,
       const vector<Geometry *> &geometry,
       const vector<Object *> &objects);

  /* Building process. */
  virtual BVHNode *widen_children_nodes(const BVHNode *root);

  /* pack */
  void pack_nodes(const BVHNode *root);

  void pack_leaf(const BVHStackEntry &e, const LeafNode *leaf);
  void pack_inner(const BVHStackEntry &e, const BVHStackEntry &e0, const BVHStackEntry &e1);

  void pack_aligned_inner(const BVHStackEntry &e,
                          const BVHStackEntry &e0,
                          const BVHStackEntry &e1);
  void pack_aligned_node(int idx,
                         const BoundBox &b0,
                         const BoundBox &b1,
                         int c0,
                         int c1,
                         uint visibility0,
                         uint visibility1);

  void pack_unaligned_inner(const BVHStackEntry &e,
                            const BVHStackEntry &e0,
                            const BVHStackEntry &e1);
  void pack_unaligned_node(int idx,
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
  void refit_node(int idx, bool leaf, BoundBox &bbox, uint &visibility);

  /* Refit range of primitives. */
  void refit_primitives(int start, int end, BoundBox &bbox, uint &visibility);

  /* triangles and strands */
  void pack_primitives();
  void pack_triangle(int idx, float4 storage[3]);

  /* merge instance BVH's */
  void pack_instances(size_t nodes_size, size_t leaf_nodes_size);
};

CCL_NAMESPACE_END

#endif /* __BVH2_H__ */
