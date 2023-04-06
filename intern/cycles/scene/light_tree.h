/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __LIGHT_TREE_H__
#define __LIGHT_TREE_H__

#include "scene/light.h"
#include "scene/scene.h"

#include "util/boundbox.h"
#include "util/task.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Orientation Bounds
 *
 * Bounds the normal axis of the lights,
 * along with their emission profiles */
struct OrientationBounds {
  float3 axis;   /* normal axis of the light */
  float theta_o; /* angle bounding the normals */
  float theta_e; /* angle bounding the light emissions */

  __forceinline OrientationBounds() {}

  __forceinline OrientationBounds(const float3 &axis_, float theta_o_, float theta_e_)
      : axis(axis_), theta_o(theta_o_), theta_e(theta_e_)
  {
  }

  enum empty_t { empty = 0 };

  /* If the orientation bound is set to empty, the values are set to minimums
   * so that merging it with another non-empty orientation bound guarantees that
   * the return value is equal to non-empty orientation bound. */
  __forceinline OrientationBounds(empty_t)
      : axis(make_float3(0, 0, 0)), theta_o(FLT_MIN), theta_e(FLT_MIN)
  {
  }

  __forceinline bool is_empty() const
  {
    return is_zero(axis);
  }

  float calculate_measure() const;
};

OrientationBounds merge(const OrientationBounds &cone_a, const OrientationBounds &cone_b);

/* --------------------------------------------------------------------
 * Light Tree Construction
 *
 * The light tree construction is based on PBRT's BVH construction.
 */

/* Light Tree uses the bounding box, the orientation bounding cone, and the energy of a cluster to
 * compute the Surface Area Orientation Heuristic (SAOH). */
struct LightTreeMeasure {
  BoundBox bbox = BoundBox::empty;
  OrientationBounds bcone = OrientationBounds::empty;
  float energy = 0.0f;

  enum empty_t { empty = 0 };

  __forceinline LightTreeMeasure() = default;

  __forceinline LightTreeMeasure(empty_t) {}

  __forceinline LightTreeMeasure(const BoundBox &bbox,
                                 const OrientationBounds &bcone,
                                 const float &energy)
      : bbox(bbox), bcone(bcone), energy(energy)
  {
  }

  __forceinline LightTreeMeasure(const LightTreeMeasure &other)
      : bbox(other.bbox), bcone(other.bcone), energy(other.energy)
  {
  }

  __forceinline bool is_zero() const
  {
    return energy == 0;
  }

  __forceinline void add(const LightTreeMeasure &measure)
  {
    if (!measure.is_zero()) {
      bbox.grow(measure.bbox);
      bcone = merge(bcone, measure.bcone);
      energy += measure.energy;
    }
  }

  /* Taken from Eq. 2 in the paper. */
  __forceinline float calculate()
  {
    float area = bbox.area();
    float area_measure = area == 0 ? len(bbox.size()) : area;
    return energy * area_measure * bcone.calculate_measure();
  }
};

LightTreeMeasure operator+(const LightTreeMeasure &a, const LightTreeMeasure &b);

/* Light Tree Emitter
 * Struct that indexes into the scene's triangle and light arrays. */
struct LightTreeEmitter {
  /* `prim_id >= 0` is an index into an object's local triangle index,
   * otherwise `-prim_id-1`(`~prim`) is an index into device lights array. */
  int prim_id;
  int object_id;
  float3 centroid;

  LightTreeMeasure measure;

  LightTreeEmitter(Scene *scene, int prim_id, int object_id);

  __forceinline bool is_triangle() const
  {
    return prim_id >= 0;
  };
};

/* Light Tree Bucket
 * Struct used to determine splitting costs in the light BVH. */
struct LightTreeBucket {
  LightTreeMeasure measure;
  int count = 0;
  static const int num_buckets = 12;

  LightTreeBucket() = default;

  LightTreeBucket(const LightTreeMeasure &measure, const int &count)
      : measure(measure), count(count)
  {
  }

  void add(const LightTreeEmitter &emitter)
  {
    measure.add(emitter.measure);
    count++;
  }
};

LightTreeBucket operator+(const LightTreeBucket &a, const LightTreeBucket &b);

/* Light Tree Node */
struct LightTreeNode {
  LightTreeMeasure measure;
  uint bit_trail;
  int num_emitters = -1; /* The number of emitters a leaf node stores. A negative number indicates
                            it is an inner node. */
  int first_emitter_index;               /* Leaf nodes contain an index to first emitter. */
  unique_ptr<LightTreeNode> children[2]; /* Inner node has two children. */

  LightTreeNode() = default;

  LightTreeNode(const LightTreeMeasure &measure, const uint &bit_trial)
      : measure(measure), bit_trail(bit_trial)
  {
  }

  __forceinline void add(const LightTreeEmitter &emitter)
  {
    measure.add(emitter.measure);
  }

  void make_leaf(const int &first_emitter_index, const int &num_emitters)
  {
    this->first_emitter_index = first_emitter_index;
    this->num_emitters = num_emitters;
  }

  __forceinline bool is_leaf() const
  {
    return num_emitters >= 0;
  }
};

/* Light BVH
 *
 * BVH-like data structure that keeps track of lights
 * and considers additional orientation and energy information */
class LightTree {
  unique_ptr<LightTreeNode> root_;

  vector<LightTreeEmitter> emitters_;
  vector<LightTreeEmitter> distant_lights_;

  Progress &progress_;

  uint max_lights_in_leaf_;

 public:
  std::atomic<int> num_nodes = 0;
  size_t num_triangles = 0;

  /* Left or right child of an inner node. */
  enum Child {
    left = 0,
    right = 1,
  };

  LightTree(Scene *scene, DeviceScene *dscene, Progress &progress, uint max_lights_in_leaf);

  /* Returns a pointer to the root node. */
  LightTreeNode *build(Scene *scene, DeviceScene *dscene);

  /* NOTE: Always use this function to create a new node so the number of nodes is in sync. */
  unique_ptr<LightTreeNode> create_node(const LightTreeMeasure &measure, const uint &bit_trial)
  {
    num_nodes++;
    return make_unique<LightTreeNode>(measure, bit_trial);
  }

  size_t num_emitters()
  {
    return emitters_.size();
  }

  const LightTreeEmitter &get_emitter(int index) const
  {
    return emitters_.at(index);
  }

 private:
  /* Thread. */
  TaskPool task_pool;
  /* Do not spawn a thread if less than this amount of emitters are to be processed. */
  enum { MIN_EMITTERS_PER_THREAD = 4096 };

  void recursive_build(Child child,
                       LightTreeNode *parent,
                       int start,
                       int end,
                       LightTreeEmitter *emitters,
                       uint bit_trail,
                       int depth);

  bool should_split(LightTreeEmitter *emitters,
                    const int start,
                    int &middle,
                    const int end,
                    LightTreeMeasure &measure,
                    int &split_dim);
};

CCL_NAMESPACE_END

#endif /* __LIGHT_TREE_H__ */
