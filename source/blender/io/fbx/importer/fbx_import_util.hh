/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "ufbx.h"

namespace blender {

struct ID;
struct Object;
struct Key;
struct KeyBlock;
struct Material;
struct bPoseChannel;

namespace io::fbx {

const char *get_fbx_name(const ufbx_string &name, const char *def = "Untitled");

ufbx_matrix camera_light_transform_to_matrix(const ufbx_transform &transform,
                                             const ufbx_node &node);

void expand_shape_key_range(KeyBlock &block, float min_weight, float max_weight);

struct FbxShapeKey {
  Key *key;
  KeyBlock *block;
};

struct FbxSkinnedMesh {
  const ufbx_node *node;
  Object *object;
  Object *armature;
  const ufbx_skin_cluster *bind_cluster;
};

struct FbxElementMapping {
  Set<Object *> imported_objects;
  Map<const ufbx_element *, Object *> el_to_object;
  Map<const ufbx_element *, FbxShapeKey> el_to_shape_key;
  Map<const ufbx_material *, Material *> mat_to_material;
  Map<const ufbx_node *, Object *> bone_to_armature;
  Vector<FbxSkinnedMesh> skinned_meshes;
  Set<const Object *> meshes_reparented_to_armature;

  /* For the armatures we create, for different use cases we need transform
   * from world space to the root bone, either in posed transform or in
   * node transform. */
  Map<const Object *, ufbx_matrix> armature_world_to_arm_pose_matrix;
  Map<const Object *, ufbx_matrix> armature_world_to_arm_node_matrix;

  /* Which FBX bone nodes got turned into actual armature bones (not all of them
   * always are; in some cases root bone is the armature object itself). */
  Set<const ufbx_node *> node_is_blender_bone;

  /* Mapping of ufbx node to object name used within blender. If names are too long
   * or duplicate, they might not match what was in FBX file. */
  Map<const ufbx_node *, std::string> node_to_name;
  /* Bone node to "bind matrix", i.e. matrix that transforms from bone (in skin bind pose) local
   * space to world space. This records bone pose or skin cluster bind matrix (skin cluster taking
   * precedence if it exists). */
  Map<const ufbx_node *, ufbx_matrix> bone_to_bind_matrix;
  Map<const ufbx_node *, ufbx_real> bone_to_length;
  /* Bone orientation correction (only rotations, only multiples of 90 degrees).
   * `blender_matrix = fbx_matrix * correction_matrix` */
  Map<const ufbx_node *, ufbx_matrix> bone_to_correction;
  Set<const ufbx_node *> bone_is_skinned;
  ufbx_matrix global_conv_matrix;

  ufbx_matrix get_node_bind_matrix(const ufbx_node *node) const
  {
    return this->bone_to_bind_matrix.lookup_default(node, node->geometry_to_world);
  }

  const ufbx_matrix *find_bone_correction(const ufbx_node *node) const
  {
    return this->bone_to_correction.lookup_ptr(node);
  }

  /* For objects parented to a bone: matrix that goes from the bone tail (bone parent space) back
   * to the FBX bone node space, i.e. undoes bone length offset and orientation correction. */
  ufbx_matrix calc_bone_child_offset_matrix(const ufbx_node *bone_node) const
  {
    const ufbx_matrix *correction = this->find_bone_correction(bone_node);
    ufbx_matrix offset_mtx = correction ? ufbx_matrix_invert(correction) : ufbx_identity_matrix;
    offset_mtx.cols[3].y = -this->bone_to_length.lookup_default(bone_node, 0.0);
    return offset_mtx;
  }

  ufbx_matrix calc_local_bind_matrix(const ufbx_node *bone_node,
                                     const ufbx_matrix &world_to_arm) const
  {
    ufbx_matrix res = this->get_node_bind_matrix(bone_node);
    ufbx_matrix parent_inv_mtx;
    if (bone_node->parent != nullptr && !bone_node->parent->is_root) {
      ufbx_matrix parent_mtx = this->get_node_bind_matrix(bone_node->parent);
      parent_inv_mtx = ufbx_matrix_invert(&parent_mtx);
    }
    else {
      parent_inv_mtx = world_to_arm;
    }
    res = ufbx_matrix_mul(&parent_inv_mtx, &res);
    return res;
  }
};

void matrix_to_m44(const ufbx_matrix &src, float dst[4][4]);
void ufbx_matrix_to_obj(const ufbx_matrix &mtx, Object *obj);
void node_matrix_to_obj(const ufbx_node *node, Object *obj, const FbxElementMapping &mapping);
void read_custom_properties(const ufbx_props &props, ID &id, bool enums_as_strings);
void read_custom_properties(const ufbx_props &props, bPoseChannel &pchan, bool enums_as_strings);

ufbx_matrix calc_bone_pose_matrix(const ufbx_transform &local_xform,
                                  const ufbx_node &node,
                                  const ufbx_matrix &local_bind_inv_matrix,
                                  const ufbx_matrix *correction);

//@TODO remove debug file print once things are working properly
// #define FBX_DEBUG_PRINT

#ifdef FBX_DEBUG_PRINT
extern FILE *g_debug_file;

inline double adjf(double f)
{
  if (fabs(f) < 0.0005) {
    return 0.0;
  }
  return f;
}

void print_matrix(const ufbx_matrix &m);
#endif

}  // namespace io::fbx
}  // namespace blender
