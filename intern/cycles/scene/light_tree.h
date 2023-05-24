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

#include <variant>

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
    if (is_zero()) {
      return 0.0f;
    }

    float area = bbox.area();
    float area_measure = area == 0 ? len(bbox.size()) : area;
    return energy * area_measure * bcone.calculate_measure();
  }

  __forceinline void reset()
  {
    *this = {};
  }

  bool transform(const Transform &tfm)
  {
    float scale_squared;
    if (transform_uniform_scale(tfm, scale_squared)) {
      bbox = bbox.transformed(&tfm);
      bcone.axis = transform_direction(&tfm, bcone.axis) * inversesqrtf(scale_squared);
      energy *= scale_squared;
      return true;
    }
    return false;
  }
};

LightTreeMeasure operator+(const LightTreeMeasure &a, const LightTreeMeasure &b);

struct LightTreeNode;

/* Light Linking. */
struct LightTreeLightLink {
  /* Bitmask for membership of primitives in this node. */
  uint64_t set_membership = 0;

  /* When all primitives below this node have identical light set membership, this
   * part of the light tree can be shared between specialized trees. */
  bool shareable = true;
  int shared_node_index = -1;

  LightTreeLightLink() = default;
  LightTreeLightLink(const uint64_t set_membership) : set_membership(set_membership) {}

  void add(const uint64_t prim_set_membership)
  {
    if (set_membership == 0) {
      set_membership = prim_set_membership;
    }
    else if (prim_set_membership != set_membership) {
      set_membership |= prim_set_membership;
      shareable = false;
    }
  }

  void add(const LightTreeLightLink &other)
  {
    if (set_membership == 0) {
      set_membership = other.set_membership;
      shareable = other.shareable;
    }
    else if (other.set_membership != set_membership) {
      set_membership |= other.set_membership;
      shareable = false;
    }
    else if (!other.shareable) {
      shareable = false;
    }
  }
};

LightTreeLightLink operator+(const LightTreeLightLink &a, const LightTreeLightLink &b);

/* Light Tree Emitter
 * An emitter is a built-in light, an emissive mesh, or an emissive triangle. */
struct LightTreeEmitter {
  /* If the emitter is a mesh, point to the root node of its subtree. */
  unique_ptr<LightTreeNode> root;

  union {
    int light_id; /* Index into device lights array. */
    int prim_id;  /* Index into an object's local triangle index. */
  };

  int object_id;
  float3 centroid;
  uint64_t light_set_membership;

  LightTreeMeasure measure;

  LightTreeEmitter(Object *object, int object_id); /* Mesh emitter. */
  LightTreeEmitter(Scene *scene, int prim_id, int object_id, bool with_transformation = false);

  __forceinline bool is_mesh() const
  {
    return root != nullptr;
  };

  __forceinline bool is_triangle() const
  {
    return !is_mesh() && prim_id >= 0;
  };

  __forceinline bool is_light() const
  {
    return !is_mesh() && light_id < 0;
  };
};

/* Light Tree Bucket
 * Struct used to determine splitting costs in the light BVH. */
struct LightTreeBucket {
  LightTreeMeasure measure;
  LightTreeLightLink light_link;
  int count = 0;
  static const int num_buckets = 12;

  LightTreeBucket() = default;

  LightTreeBucket(const LightTreeMeasure &measure,
                  const LightTreeLightLink &light_link,
                  const int &count)
      : measure(measure), light_link(light_link), count(count)
  {
  }

  void add(const LightTreeEmitter &emitter)
  {
    measure.add(emitter.measure);
    light_link.add(emitter.light_set_membership);
    count++;
  }
};

LightTreeBucket operator+(const LightTreeBucket &a, const LightTreeBucket &b);

/* Light Tree Node */
struct LightTreeNode {
  LightTreeMeasure measure;
  LightTreeLightLink light_link;
  uint bit_trail;
  int object_id;

  /* A bitmask of `LightTreeNodeType`, as in the building process an instance node can also be a
   * leaf or an inner node. */
  int type;

  struct Leaf {
    /* The number of emitters a leaf node stores. */
    int num_emitters = -1;
    /* Index to first emitter. */
    int first_emitter_index = -1;
  };

  struct Inner {
    /* Inner node has two children. */
    unique_ptr<LightTreeNode> children[2];
  };

  struct Instance {
    LightTreeNode *reference = nullptr;
  };

  std::variant<Leaf, Inner, Instance> variant_type;

  LightTreeNode(const LightTreeMeasure &measure, const uint &bit_trial)
      : measure(measure), bit_trail(bit_trial), variant_type(Inner())
  {
    type = LIGHT_TREE_INNER;
  }

  ~LightTreeNode() = default;

  __forceinline void add(const LightTreeEmitter &emitter)
  {
    measure.add(emitter.measure);
  }

  __forceinline Leaf &get_leaf()
  {
    return std::get<Leaf>(variant_type);
  }

  __forceinline const Leaf &get_leaf() const
  {
    return std::get<Leaf>(variant_type);
  }

  __forceinline Inner &get_inner()
  {
    return std::get<Inner>(variant_type);
  }

  __forceinline const Inner &get_inner() const
  {
    return std::get<Inner>(variant_type);
  }

  __forceinline Instance &get_instance()
  {
    return std::get<Instance>(variant_type);
  }

  __forceinline const Instance &get_instance() const
  {
    return std::get<Instance>(variant_type);
  }

  void make_leaf(const int first_emitter_index, const int num_emitters)
  {
    variant_type = Leaf();
    Leaf &leaf = get_leaf();

    leaf.first_emitter_index = first_emitter_index;
    leaf.num_emitters = num_emitters;
    type = LIGHT_TREE_LEAF;
  }

  void make_distant(const int first_emitter_index, const int num_emitters)
  {
    variant_type = Leaf();
    Leaf &leaf = get_leaf();

    leaf.first_emitter_index = first_emitter_index;
    leaf.num_emitters = num_emitters;
    type = LIGHT_TREE_DISTANT;
  }

  void make_instance(LightTreeNode *reference, const int object_id)
  {
    variant_type = Instance();
    Instance &instance = get_instance();

    instance.reference = reference;
    this->object_id = object_id;
    type = LIGHT_TREE_INSTANCE;
  }

  LightTreeNode *get_reference()
  {
    assert(is_instance());
    if (type == LIGHT_TREE_INSTANCE) {
      return get_instance().reference;
    }
    return this;
  }

  __forceinline bool is_instance() const
  {
    return type & LIGHT_TREE_INSTANCE;
  }

  __forceinline bool is_leaf() const
  {
    return type & LIGHT_TREE_LEAF;
  }

  __forceinline bool is_inner() const
  {
    return type & LIGHT_TREE_INNER;
  }

  __forceinline bool is_distant() const
  {
    return type == LIGHT_TREE_DISTANT;
  }
};

/* Light BVH
 *
 * BVH-like data structure that keeps track of lights
 * and considers additional orientation and energy information */
class LightTree {
  unique_ptr<LightTreeNode> root_;

  /* Local lights, distant lights and mesh lights are added to separate vectors for light tree
   * construction. They are all considered as `emitters_`. */
  vector<LightTreeEmitter> emitters_;
  vector<LightTreeEmitter> local_lights_;
  vector<LightTreeEmitter> distant_lights_;
  vector<LightTreeEmitter> mesh_lights_;

  std::unordered_map<Mesh *, int> offset_map_;

  Progress &progress_;

  uint max_lights_in_leaf_;

 public:
  std::atomic<int> num_nodes = 0;
  size_t num_triangles = 0;

  /* Bitmask of receiver light sets used. Default set is always used. */
  uint64_t light_link_receiver_used = 1;

  /* An inner node itself or its left and right child. */
  enum Child {
    self = -1,
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

  const LightTreeEmitter *get_emitters() const
  {
    return emitters_.data();
  }

 private:
  /* Thread. */
  TaskPool task_pool;
  /* Do not spawn a thread if less than this amount of emitters are to be processed. */
  enum { MIN_EMITTERS_PER_THREAD = 4096 };

  void recursive_build(Child child,
                       LightTreeNode *inner,
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
                    LightTreeLightLink &light_link,
                    int &split_dim);

  /* Check whether the light tree can use this triangle as light-emissive. */
  bool triangle_usable_as_light(Mesh *mesh, int prim_id);

  /* Add all the emissive triangles of a mesh to the light tree. */
  void add_mesh(Scene *scene, Mesh *mesh, int object_id);
};

CCL_NAMESPACE_END

#endif /* __LIGHT_TREE_H__ */
