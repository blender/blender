/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __LIGHT_TREE_H__
#define __LIGHT_TREE_H__

#include "scene/light.h"
#include "scene/scene.h"

#include "util/boundbox.h"
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

  __forceinline OrientationBounds()
  {
  }

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

  float calculate_measure() const;
};

OrientationBounds merge(const OrientationBounds &cone_a, const OrientationBounds &cone_b);

/* --------------------------------------------------------------------
 * Light Tree Construction
 *
 * The light tree construction is based on PBRT's BVH construction.
 */

/* Light Tree Primitive
 * Struct that indexes into the scene's triangle and light arrays. */
struct LightTreePrimitive {
  /* `prim_id >= 0` is an index into an object's local triangle index,
   * otherwise `-prim_id-1`(`~prim`) is an index into device lights array. */
  int prim_id;
  int object_id;

  float energy;
  float3 centroid;
  OrientationBounds bcone;
  BoundBox bbox;

  LightTreePrimitive(Scene *scene, int prim_id, int object_id);

  inline bool is_triangle() const
  {
    return prim_id >= 0;
  };
};

/* Light Tree Bucket Info
 * Struct used to determine splitting costs in the light BVH. */
struct LightTreeBucketInfo {
  LightTreeBucketInfo()
      : energy(0.0f), bbox(BoundBox::empty), bcone(OrientationBounds::empty), count(0)
  {
  }

  float energy; /* Total energy in the partition */
  BoundBox bbox;
  OrientationBounds bcone;
  int count;

  static const int num_buckets = 12;
};

/* Light Tree Node */
struct LightTreeNode {
  BoundBox bbox;
  OrientationBounds bcone;
  float energy;
  uint bit_trail;
  int num_prims = -1;
  union {
    int first_prim_index;  /* leaf nodes contain an index to first primitive. */
    int right_child_index; /* interior nodes contain an index to second child. */
  };
  LightTreeNode() = default;

  LightTreeNode(const BoundBox &bbox,
                const OrientationBounds &bcone,
                const float &energy,
                const uint &bit_trial)
      : bbox(bbox), bcone(bcone), energy(energy), bit_trail(bit_trial)
  {
  }

  void make_leaf(const uint &first_prim_index, const int &num_prims)
  {
    this->first_prim_index = first_prim_index;
    this->num_prims = num_prims;
  }
  void make_interior(const int &right_child_index)
  {
    this->right_child_index = right_child_index;
  }

  inline bool is_leaf() const
  {
    return num_prims >= 0;
  }
};

/* Light BVH
 *
 * BVH-like data structure that keeps track of lights
 * and considers additional orientation and energy information */
class LightTree {
  vector<LightTreeNode> nodes_;
  uint max_lights_in_leaf_;

 public:
  LightTree(vector<LightTreePrimitive> &prims,
            const int &num_distant_lights,
            uint max_lights_in_leaf);

  const vector<LightTreeNode> &get_nodes() const;

 private:
  int recursive_build(
      int start, int end, vector<LightTreePrimitive> &prims, uint bit_trail, int depth);
  float min_split_saoh(const BoundBox &centroid_bbox,
                       int start,
                       int end,
                       const BoundBox &bbox,
                       const OrientationBounds &bcone,
                       int &split_dim,
                       int &split_bucket,
                       int &num_left_prims,
                       const vector<LightTreePrimitive> &prims);
};

CCL_NAMESPACE_END

#endif /* __LIGHT_TREE_H__ */
