/* SPDX-License-Identifier: Apache-2.0
 * Adapted from code copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011-2022 Blender Foundation. */

#ifndef __BVH_NODE_H__
#define __BVH_NODE_H__

#include "util/boundbox.h"
#include "util/types.h"

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
  virtual ~BVHNode()
  {
    delete aligned_space;
  }

  virtual bool is_leaf() const = 0;
  virtual int num_children() const = 0;
  virtual BVHNode *get_child(int i) const = 0;
  virtual int num_triangles() const
  {
    return 0;
  }
  virtual void print(int depth = 0) const = 0;

  inline void set_aligned_space(const Transform &aligned_space)
  {
    is_unaligned = true;
    if (this->aligned_space == NULL) {
      this->aligned_space = new Transform(aligned_space);
    }
    else {
      *this->aligned_space = aligned_space;
    }
  }

  inline Transform get_aligned_space() const
  {
    if (aligned_space == NULL) {
      return transform_identity();
    }
    return *aligned_space;
  }

  inline bool has_unaligned() const
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
  float computeSubtreeSAHCost(const BVHParams &p, float probability = 1.0f) const;
  void deleteSubtree();

  uint update_visibility();
  void update_time();

  /* Dump the content of the tree as a graphviz file. */
  void dump_graph(const char *filename);

  // Properties.
  BoundBox bounds;
  uint visibility;

  bool is_unaligned;

  /* TODO(sergey): Can be stored as 3x3 matrix, but better to have some
   * utilities and type defines in util_transform first.
   */
  Transform *aligned_space;

  float time_from, time_to;

 protected:
  explicit BVHNode(const BoundBox &bounds)
      : bounds(bounds),
        visibility(0),
        is_unaligned(false),
        aligned_space(NULL),
        time_from(0.0f),
        time_to(1.0f)
  {
  }

  explicit BVHNode(const BVHNode &other)
      : bounds(other.bounds),
        visibility(other.visibility),
        is_unaligned(other.is_unaligned),
        aligned_space(NULL),
        time_from(other.time_from),
        time_to(other.time_to)
  {
    if (other.aligned_space != NULL) {
      assert(other.is_unaligned);
      aligned_space = new Transform();
      *aligned_space = *other.aligned_space;
    }
    else {
      assert(!other.is_unaligned);
    }
  }
};

class InnerNode : public BVHNode {
 public:
  static constexpr int kNumMaxChildren = 8;

  InnerNode(const BoundBox &bounds, BVHNode *child0, BVHNode *child1)
      : BVHNode(bounds), num_children_(2)
  {
    children[0] = child0;
    children[1] = child1;
    reset_unused_children();

    if (child0 && child1) {
      visibility = child0->visibility | child1->visibility;
    }
    else {
      /* Happens on build cancel. */
      visibility = 0;
    }
  }

  InnerNode(const BoundBox &bounds, BVHNode **children, const int num_children)
      : BVHNode(bounds), num_children_(num_children)
  {
    visibility = 0;
    time_from = FLT_MAX;
    time_to = -FLT_MAX;
    for (int i = 0; i < num_children; ++i) {
      assert(children[i] != NULL);
      visibility |= children[i]->visibility;
      this->children[i] = children[i];
      time_from = min(time_from, children[i]->time_from);
      time_to = max(time_to, children[i]->time_to);
    }
    reset_unused_children();
  }

  /* NOTE: This function is only used during binary BVH builder, and it's
   * supposed to be configured to have 2 children which will be filled-in in a
   * bit. But this is important to have children reset to NULL. */
  explicit InnerNode(const BoundBox &bounds) : BVHNode(bounds), num_children_(0)
  {
    reset_unused_children();
    visibility = 0;
    num_children_ = 2;
  }

  bool is_leaf() const
  {
    return false;
  }
  int num_children() const
  {
    return num_children_;
  }
  BVHNode *get_child(int i) const
  {
    assert(i >= 0 && i < num_children_);
    return children[i];
  }
  void print(int depth) const;

  int num_children_;
  BVHNode *children[kNumMaxChildren];

 protected:
  void reset_unused_children()
  {
    for (int i = num_children_; i < kNumMaxChildren; ++i) {
      children[i] = NULL;
    }
  }
};

class LeafNode : public BVHNode {
 public:
  LeafNode(const BoundBox &bounds, uint visibility, int lo, int hi)
      : BVHNode(bounds), lo(lo), hi(hi)
  {
    this->bounds = bounds;
    this->visibility = visibility;
  }

  LeafNode(const LeafNode &other) : BVHNode(other), lo(other.lo), hi(other.hi) {}

  bool is_leaf() const
  {
    return true;
  }
  int num_children() const
  {
    return 0;
  }
  BVHNode *get_child(int) const
  {
    return NULL;
  }
  int num_triangles() const
  {
    return hi - lo;
  }
  void print(int depth) const;

  int lo;
  int hi;
};

CCL_NAMESPACE_END

#endif /* __BVH_NODE_H__ */
