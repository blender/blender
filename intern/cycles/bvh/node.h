/* SPDX-FileCopyrightText: 2009-2010 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#pragma once

#include "util/boundbox.h"
#include "util/types.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

enum BVH_STAT {
  BVH_STAT_NODE_COUNT,
  BVH_STAT_INNER_COUNT,
  BVH_STAT_LEAF_COUNT,
  BVH_STAT_TRIANGLE_COUNT,
  BVH_STAT_CHILDNODE_COUNT,
  BVH_STAT_ALIGNED_COUNT,
  BVH_STAT_UNALIGNED_COUNT,
  BVH_STAT_ALIGNED_INNER_COUNT,
  BVH_STAT_UNALIGNED_INNER_COUNT,
  BVH_STAT_ALIGNED_LEAF_COUNT,
  BVH_STAT_UNALIGNED_LEAF_COUNT,
  BVH_STAT_DEPTH,
};

class BVHParams;

class BVHNode {
 public:
  virtual ~BVHNode() = default;

  virtual bool is_leaf() const = 0;
  virtual int num_children() const = 0;
  virtual BVHNode *get_child(const int i) const = 0;
  virtual int num_triangles() const
  {
    return 0;
  }
  virtual void print(const int depth = 0) const = 0;

  void set_aligned_space(const Transform &aligned_space)
  {
    is_unaligned = true;
    if (this->aligned_space == nullptr) {
      this->aligned_space = make_unique<Transform>(aligned_space);
    }
    else {
      *this->aligned_space = aligned_space;
    }
  }

  Transform get_aligned_space() const
  {
    if (aligned_space == nullptr) {
      return transform_identity();
    }
    return *aligned_space;
  }

  bool has_unaligned() const
  {
    if (is_leaf()) {
      return false;
    }
    for (int i = 0; i < num_children(); ++i) {
      if (get_child(i)->is_unaligned) {
        return true;
      }
    }
    return false;
  }

  // Subtree functions
  int getSubtreeSize(BVH_STAT stat = BVH_STAT_NODE_COUNT) const;
  float computeSubtreeSAHCost(const BVHParams &p, const float probability = 1.0f) const;

  uint update_visibility();
  void update_time();

  /* Dump the content of the tree as a graphviz file. */
  void dump_graph(const char *filename);

  // Properties.
  BoundBox bounds;
  uint visibility = 0;

  bool is_unaligned = false;

  /* TODO(sergey): Can be stored as 3x3 matrix, but better to have some
   * utilities and type defines in util_transform first.
   */
  unique_ptr<Transform> aligned_space;

  float time_from = 0.0f, time_to = 1.0f;

 protected:
  explicit BVHNode(const BoundBox &bounds) : bounds(bounds) {}

  explicit BVHNode(const BVHNode &other)
      : bounds(other.bounds),
        visibility(other.visibility),
        is_unaligned(other.is_unaligned),

        time_from(other.time_from),
        time_to(other.time_to)
  {
    if (other.aligned_space != nullptr) {
      assert(other.is_unaligned);
      aligned_space = make_unique<Transform>(*other.aligned_space);
    }
    else {
      assert(!other.is_unaligned);
    }
  }
};

class InnerNode : public BVHNode {
 public:
  static constexpr int kNumMaxChildren = 8;

  InnerNode(const BoundBox &bounds, unique_ptr<BVHNode> &&child0, unique_ptr<BVHNode> &&child1)
      : BVHNode(bounds), num_children_(2)
  {
    if (child0 && child1) {
      visibility = child0->visibility | child1->visibility;
    }
    else {
      /* Happens on build cancel. */
      visibility = 0;
    }

    children[0] = std::move(child0);
    children[1] = std::move(child1);
  }

  /* NOTE: This function is only used during binary BVH builder, and it's
   * supposed to be configured to have 2 children which will be filled-in in a
   * bit. */
  explicit InnerNode(const BoundBox &bounds) : BVHNode(bounds), num_children_(0)
  {
    visibility = 0;
    num_children_ = 2;
  }

  bool is_leaf() const override
  {
    return false;
  }
  int num_children() const override
  {
    return num_children_;
  }
  BVHNode *get_child(const int i) const override
  {
    assert(i >= 0 && i < num_children_);
    return children[i].get();
  }
  void print(const int depth) const override;

  int num_children_;
  unique_ptr<BVHNode> children[kNumMaxChildren];
};

class LeafNode : public BVHNode {
 public:
  LeafNode(const BoundBox &bounds, const uint visibility, const int lo, const int hi)
      : BVHNode(bounds), lo(lo), hi(hi)
  {
    this->bounds = bounds;
    this->visibility = visibility;
  }

  LeafNode(const LeafNode &other) = default;

  bool is_leaf() const override
  {
    return true;
  }
  int num_children() const override
  {
    return 0;
  }
  BVHNode *get_child(int /*i*/) const override
  {
    return nullptr;
  }
  int num_triangles() const override
  {
    return hi - lo;
  }
  void print(const int depth) const override;

  int lo;
  int hi;
};

CCL_NAMESPACE_END
