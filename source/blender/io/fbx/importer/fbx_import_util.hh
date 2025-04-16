/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_set.hh"

#include "ufbx.h"

struct ID;
struct Object;
struct Key;
struct Material;
struct bPoseChannel;

namespace blender::io::fbx {

const char *get_fbx_name(const ufbx_string &name, const char *def = "Untitled");

struct FbxElementMapping {
  Map<const ufbx_element *, Object *> el_to_object;
  Map<const ufbx_element *, Key *> el_to_shape_key;
  Map<const ufbx_material *, Material *> mat_to_material;
  Map<const ufbx_node *, Object *> bone_to_armature;
  /* Mapping of ufbx node to object name used within blender. If names are too long
   * or duplicate, they might not match what was in FBX file. */
  Map<const ufbx_node *, std::string> node_to_name;
  /* Bone node to "bind matrix", i.e. matrix that transforms from bone (in skin bind pose) local
   * space to world space. */
  Map<const ufbx_node *, ufbx_matrix> bone_to_bind_matrix;
  Map<const ufbx_node *, ufbx_real> bone_to_length;
  /* Which bones actually have pose or skin cluster bind matrices in the FBX file (the others
   * would just use their world transform). */
  Set<const ufbx_node *> bone_has_pose_or_skin_matrix;
  Set<const ufbx_node *> bone_is_skinned;
  ufbx_matrix global_conv_matrix;

  //@TODO: these could be precalculated once
  ufbx_matrix calc_local_bind_matrix(const ufbx_node *bone_node,
                                     const ufbx_matrix &world_to_arm,
                                     bool &r_found) const
  {
    r_found = false;
    const ufbx_matrix *bind_mtx = this->bone_to_bind_matrix.lookup_ptr(bone_node);
    if (bind_mtx == nullptr) {
      return ufbx_identity_matrix;
    }
    r_found = true;
    ufbx_matrix res = *bind_mtx;

    const ufbx_matrix *parent_mtx = nullptr;
    if (bone_node->parent != nullptr) {
      parent_mtx = this->bone_to_bind_matrix.lookup_ptr(bone_node->parent);
    }

    ufbx_matrix parent_inv_mtx;
    if (parent_mtx) {
      parent_inv_mtx = ufbx_matrix_invert(parent_mtx);
    }
    else {
      parent_inv_mtx = world_to_arm;
    }
    res = ufbx_matrix_mul(&parent_inv_mtx, &res);
    return res;
  }
};

void matrix_to_m44(const ufbx_matrix &src, float dst[4][4]);
void m44_to_matrix(const float src[4][4], ufbx_matrix &dst);
void ufbx_matrix_to_obj(const ufbx_matrix &mtx, Object *obj);
void node_matrix_to_obj(const ufbx_node *node, Object *obj, const FbxElementMapping &mapping);
void read_custom_properties(const ufbx_props &props, ID &id, bool enums_as_strings);
void read_custom_properties(const ufbx_props &props, bPoseChannel &pchan, bool enums_as_strings);

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

}  // namespace blender::io::fbx
