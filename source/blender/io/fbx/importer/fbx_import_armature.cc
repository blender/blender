/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLI_math_vector.hh"

#include "DNA_object_types.h"

#include "ED_armature.hh"

#include "IO_fbx.hh"

#include "fbx_import_armature.hh"

namespace blender::io::fbx {

struct ArmatureImportContext {
  Main &bmain;
  const ufbx_scene &fbx;
  const FBXImportParams &params;
  FbxElementMapping &mapping;

  ArmatureImportContext(Main &main,
                        const ufbx_scene &fbx,
                        const FBXImportParams &params,
                        FbxElementMapping &mapping)
      : bmain(main), fbx(fbx), params(params), mapping(mapping)
  {
  }

  Object *create_armature_for_node(const ufbx_node *node);
  void create_armature_bones(const ufbx_node *node,
                             Object *arm_obj,
                             Set<const ufbx_node *> &arm_bones,
                             EditBone *parent_bone,
                             const ufbx_matrix &parent_mtx,
                             const ufbx_matrix &world_to_arm,
                             const float parent_bone_size);
  void find_armatures(const ufbx_node *node);
  void calc_bone_bind_matrices();
};

Object *ArmatureImportContext::create_armature_for_node(const ufbx_node *node)
{
  Object *obj = nullptr;
  if (node != nullptr) {
    obj = this->mapping.el_to_object.lookup_default(&node->element, nullptr);
    if (obj != nullptr && obj->type == OB_ARMATURE) {
      return obj;
    }
  }

  const char *arm_name = node ? get_fbx_name(node->name, "Armature") : "Armature";
  const char *obj_name = node ? get_fbx_name(node->name, "Armature") : "Armature";
#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file, "create ARMATURE %s\n", arm_name);
#endif

  bArmature *arm = BKE_armature_add(&this->bmain, arm_name);
  obj = BKE_object_add_only_object(&this->bmain, OB_ARMATURE, obj_name);
  obj->dtx |= OB_DRAW_IN_FRONT;
  obj->data = arm;
  if (node != nullptr) {
    this->mapping.el_to_object.add(&node->element, obj);
    if (this->params.use_custom_props) {
      read_custom_properties(node->props, obj->id, this->params.props_enum_as_string);
    }
    node_matrix_to_obj(node, obj, this->mapping);
  }
  else {
    /* For armatures created at root, make them have the same rotation/scale
     * as done by ufbx for all regular nodes. */
    ufbx_matrix_to_obj(this->mapping.global_conv_matrix, obj);
  }
  return obj;
}

void ArmatureImportContext::create_armature_bones(const ufbx_node *node,
                                                  Object *arm_obj,
                                                  Set<const ufbx_node *> &arm_bones,
                                                  EditBone *parent_bone,
                                                  const ufbx_matrix &parent_mtx,
                                                  const ufbx_matrix &world_to_arm,
                                                  const float parent_bone_size)
{
  bArmature *arm = static_cast<bArmature *>(arm_obj->data);

  EditBone *bone = ED_armature_ebone_add(arm, get_fbx_name(node->name, "Bone"));
  this->mapping.node_to_name.add(node, bone->name);
  arm_bones.add(node);
  /* For all bone nodes, record the whole armature as the owning object. */
  this->mapping.el_to_object.add(&node->element, arm_obj);
  bone->flag |= BONE_SELECTED;
  bone->parent = parent_bone;

  this->mapping.bone_to_armature.add(node, arm_obj);

#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file,
          "create BONE %s (parent %s) parent_mtx:\n",
          node->name.data,
          parent_bone ? parent_bone->name : "");
  print_matrix(parent_mtx);
#endif

  const ufbx_matrix *bind_mtx = this->mapping.bone_to_bind_matrix.lookup_ptr(node);
  BLI_assert_msg(bind_mtx, "fbx: did not find bind matrix for bone");
  ufbx_matrix bone_mtx = bind_mtx ? *bind_mtx : ufbx_identity_matrix;

#ifdef FBX_DEBUG_PRINT
  // fprintf(g_debug_file, "  local_bind_mtx:\n");
  // print_matrix(bone_mtx);
#endif

  bone_mtx = ufbx_matrix_mul(&world_to_arm, &bone_mtx);
  bone_mtx.cols[0] = ufbx_vec3_normalize(bone_mtx.cols[0]);
  bone_mtx.cols[1] = ufbx_vec3_normalize(bone_mtx.cols[1]);
  bone_mtx.cols[2] = ufbx_vec3_normalize(bone_mtx.cols[2]);
#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file, "  bone_mtx:\n");
  print_matrix(bone_mtx);
#endif

  /* Calculate bone tail position. */
  float bone_size = 0.0f;
  int child_bone_count = 0;
  for (const ufbx_node *fchild : node->children) {
    if (fchild->attrib_type != UFBX_ELEMENT_BONE) {
      continue;
    }

    /* Estimate child position from local transform, but if the child
     * is skinned/posed then use the posed transform instead. */
    ufbx_vec3 pos = fchild->local_transform.translation;
    if (this->mapping.bone_has_pose_or_skin_matrix.contains(fchild)) {
      bool found;
      ufbx_matrix local_mtx = this->mapping.calc_local_bind_matrix(fchild, world_to_arm, found);
      if (found) {
        pos = local_mtx.cols[3];
      }
    }
    bone_size += math::length(float3(pos.x, pos.y, pos.z));
    child_bone_count++;
  }
  if (child_bone_count > 0) {
    bone_size /= child_bone_count;
  }
  else {
    /* This is leaf bone, set length to parent bone length. */
    bone_size = parent_bone_size;
    /* If we do not have actual pose/skin matrix for this bone, apply local transform onto parent
     * matrix. */
    if (!this->mapping.bone_has_pose_or_skin_matrix.contains(node)) {
      ufbx_matrix offset_mtx = ufbx_transform_to_matrix(&node->local_transform);
      bone_mtx = ufbx_matrix_mul(&parent_mtx, &offset_mtx);
      bone_mtx.cols[0] = ufbx_vec3_normalize(bone_mtx.cols[0]);
      bone_mtx.cols[1] = ufbx_vec3_normalize(bone_mtx.cols[1]);
      bone_mtx.cols[2] = ufbx_vec3_normalize(bone_mtx.cols[2]);
#ifdef FBX_DEBUG_PRINT
      fprintf(g_debug_file, "  bone_mtx adj for non-posed bones:\n");
      print_matrix(bone_mtx);
#endif
    }
  }
  /* Zero length bones are automatically collapsed into their parent when you leave edit mode,
   * so enforce a minimum length. */
  bone_size = math::max(bone_size, 0.01f);
  bone->tail[0] = 0.0f;
  bone->tail[1] = bone_size;
  bone->tail[2] = 0.0f;
  this->mapping.bone_to_length.add(node, bone_size);

  /* Set bone matrix. */
  float bone_matrix[4][4];
  matrix_to_m44(bone_mtx, bone_matrix);
  ED_armature_ebone_from_mat4(bone, bone_matrix);
#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file,
          "  length %.3f head (%.3f %.3f %.3f) tail (%.3f %.3f %.3f)\n",
          adjf(bone_size),
          adjf(bone->head[0]),
          adjf(bone->head[1]),
          adjf(bone->head[2]),
          adjf(bone->tail[0]),
          adjf(bone->tail[1]),
          adjf(bone->tail[2]));
#endif

  /* Mark bone as connected to parent if head approximately in the same place as parent tail, in
   * both rest pose and current pose. */
  if (parent_bone != nullptr) {
    float3 self_head_rest(bone->head);
    float3 par_tail_rest(parent_bone->tail);
    const float connect_dist = 1.0e-4f;
    const float connect_dist_sq = connect_dist * connect_dist;
    float dist_sq_rest = math::distance_squared(self_head_rest, par_tail_rest);
    if (dist_sq_rest < connect_dist_sq) {
      /* Bones seem connected in rest pose, now check their current transforms. */
      ufbx_vec3 self_head_cur_u = node->node_to_world.cols[3];
      ufbx_vec3 par_tail = {0, parent_bone_size, 0};
      ufbx_vec3 par_tail_cur_u = ufbx_transform_position(&node->parent->node_to_world, par_tail);
      float3 self_head_cur(self_head_cur_u.x, self_head_cur_u.y, self_head_cur_u.z);
      float3 par_tail_cur(par_tail_cur_u.x, par_tail_cur_u.y, par_tail_cur_u.z);
      float dist_sq_cur = math::distance_squared(self_head_cur, par_tail_cur);

      if (dist_sq_cur < connect_dist_sq) {
        /* Connected in both cases. */
        bone->flag |= BONE_CONNECTED;
      }
    }
  }

  /* Recurse into child bones. */
  for (const ufbx_node *fchild : node->children) {
    if (fchild->attrib_type != UFBX_ELEMENT_BONE) {
      continue;
    }

    bool skip_child = false;
    if (this->params.ignore_leaf_bones) {
      if (node->children.count == 1 && fchild->children.count == 0 &&
          !mapping.bone_is_skinned.contains(fchild))
      {
        skip_child = true;
        /* We are skipping this bone, but still record as it would be belonging to
         * our armature -- so that later code does not try to create an empty for it. */
        this->mapping.el_to_object.add(&fchild->element, arm_obj);
      }
    }

    if (!skip_child) {
      create_armature_bones(fchild, arm_obj, arm_bones, bone, bone_mtx, world_to_arm, bone_size);
    }
  }
}

void ArmatureImportContext::find_armatures(const ufbx_node *node)
{
  /* Need to create armature if we are root bone, or any child is bone. */
  bool needs_arm = false;
  for (const ufbx_node *fchild : node->children) {
    if (fchild->attrib_type == UFBX_ELEMENT_BONE) {
      needs_arm = true;
      break;
    }
  }
  if (node->bone && node->bone->is_root) {
    needs_arm = true;
  }

  /* Create armature if needed. */
  if (needs_arm) {
    Object *arm_obj = nullptr;
    if ((node->bone && node->bone->is_root) || (node->attrib_type == UFBX_ELEMENT_EMPTY)) {
      arm_obj = this->create_armature_for_node(node);
    }
    else {
      arm_obj = this->create_armature_for_node(nullptr);
    }

    /* Create bones in edit mode. */
    ufbx_matrix arm_to_world;
    m44_to_matrix(arm_obj->runtime->object_to_world.ptr(), arm_to_world);
    ufbx_matrix world_to_arm = ufbx_matrix_invert(&arm_to_world);

    Set<const ufbx_node *> arm_bones;
    bArmature *arm = static_cast<bArmature *>(arm_obj->data);
    ED_armature_to_edit(arm);
    for (const ufbx_node *fchild : node->children) {
      if (fchild->attrib_type == UFBX_ELEMENT_BONE) {
        create_armature_bones(
            fchild, arm_obj, arm_bones, nullptr, ufbx_identity_matrix, world_to_arm, 1.0f);
      }
    }
    ED_armature_from_edit(&this->bmain, arm);
    ED_armature_edit_free(arm);

    /* Setup pose on the object, and custom properties on the pose bones. */
    for (const ufbx_node *fbone : arm_bones) {
      bPoseChannel *pchan = BKE_pose_channel_find_name(
          arm_obj->pose, this->mapping.node_to_name.lookup_default(fbone, "").c_str());
      if (pchan == nullptr) {
        continue;
      }
      read_custom_properties(fbone->props, *pchan, this->params.props_enum_as_string);

      /* For bones that have rest/bind information, put their current transform into
       * the current pose. */
      if (this->mapping.bone_has_pose_or_skin_matrix.contains(fbone)) {
        bool found;
        ufbx_matrix bind_local_mtx = this->mapping.calc_local_bind_matrix(
            fbone, world_to_arm, found);
        if (found) {
          ufbx_matrix bind_local_mtx_inv = ufbx_matrix_invert(&bind_local_mtx);
          ufbx_matrix local_mtx = fbone->node_to_parent;
          if (fbone->node_depth <= 1) {
            local_mtx = ufbx_matrix_mul(&world_to_arm, &fbone->node_to_world);
          }
          ufbx_matrix pose_mtx = ufbx_matrix_mul(&bind_local_mtx_inv, &local_mtx);

          float pchan_matrix[4][4];
          matrix_to_m44(pose_mtx, pchan_matrix);
          BKE_pchan_apply_mat4(pchan, pchan_matrix, false);

#ifdef FBX_DEBUG_PRINT
          fprintf(g_debug_file, "set POSE matrix of %s matrix_basis:\n", fbone->name.data);
          print_matrix(pose_mtx);
#endif
        }
      }
    }
  }

  /* Recurse into non-bone children. */
  for (const ufbx_node *fchild : node->children) {
    if (fchild->attrib_type != UFBX_ELEMENT_BONE) {
      this->find_armatures(fchild);
    }
  }
}

void ArmatureImportContext::calc_bone_bind_matrices()
{
  /* Figure out bind matrices for bone nodes:
   * - Get them from "pose" objects in FBX that are marked as "bind pose",
   * - From all "skin deformer" objects in FBX; these override the ones from "poses".
   * - For all the bone nodes that do not have a matrix yet, record their world matrix
   *   as bind matrix. */
  for (const ufbx_pose *fpose : this->fbx.poses) {
    if (!fpose->is_bind_pose) {
      continue;
    }
    for (const ufbx_bone_pose &bone_pose : fpose->bone_poses) {
      const ufbx_matrix &bind_matrix = bone_pose.bone_to_world;
      this->mapping.bone_to_bind_matrix.add_overwrite(bone_pose.bone_node, bind_matrix);
      this->mapping.bone_has_pose_or_skin_matrix.add(bone_pose.bone_node);
#ifdef FBX_DEBUG_PRINT
      fprintf(g_debug_file, "bone POSE matrix %s\n", bone_pose.bone_node->name.data);
      print_matrix(bind_matrix);
#endif
    }
  }

  for (const ufbx_skin_deformer *fskin : this->fbx.skin_deformers) {
    for (const ufbx_skin_cluster *fbone : fskin->clusters) {
      const ufbx_matrix &bind_matrix = fbone->bind_to_world;
      this->mapping.bone_to_bind_matrix.add_overwrite(fbone->bone_node, bind_matrix);
      this->mapping.bone_has_pose_or_skin_matrix.add(fbone->bone_node);
      this->mapping.bone_is_skinned.add(fbone->bone_node);
#ifdef FBX_DEBUG_PRINT
      fprintf(g_debug_file, "bone SKIN matrix %s\n", fbone->bone_node->name.data);
      print_matrix(bind_matrix);
#endif
    }
  }

  for (const ufbx_bone *fbone : this->fbx.bones) {
    if (fbone->instances.count != 0) {
      const ufbx_node *bone_node = fbone->instances[0];
      const ufbx_matrix &bind_matrix = bone_node->node_to_world;
      this->mapping.bone_to_bind_matrix.add(bone_node, bind_matrix);
#ifdef FBX_DEBUG_PRINT
      fprintf(g_debug_file, "bone NODE matrix %s\n", bone_node->name.data);
      print_matrix(bind_matrix);
#endif
    }
  }
}

void import_armatures(Main &bmain,
                      const ufbx_scene &fbx,
                      FbxElementMapping &mapping,
                      const FBXImportParams &params)
{
  ArmatureImportContext context(bmain, fbx, params, mapping);
  context.calc_bone_bind_matrices();

  /* Create blender armatures at:
   * - "Root" bones,
   * - Bones with an empty parent,
   * - For bones without a parent or a non-empty parent, create an armature above them. */
  context.find_armatures(fbx.root_node);
}

}  // namespace blender::io::fbx
