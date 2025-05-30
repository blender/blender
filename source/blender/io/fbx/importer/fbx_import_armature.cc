/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"

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
                             const Set<const ufbx_node *> &bone_nodes,
                             EditBone *parent_bone,
                             const ufbx_matrix &parent_mtx,
                             const ufbx_matrix &world_to_arm,
                             const float parent_bone_size);
  void find_armatures(const ufbx_node *node);
  void calc_bone_bind_matrices();
};

Object *ArmatureImportContext::create_armature_for_node(const ufbx_node *node)
{
  BLI_assert_msg(node != nullptr, "fbx: node for armature creation should not be null");

  const char *arm_name = get_fbx_name(node->name, "Armature");
  const char *obj_name = get_fbx_name(node->name, "Armature");
#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file, "create ARMATURE %s\n", arm_name);
#endif

  bArmature *arm = BKE_armature_add(&this->bmain, arm_name);
  Object *obj = BKE_object_add_only_object(&this->bmain, OB_ARMATURE, obj_name);
  obj->dtx |= OB_DRAW_IN_FRONT;
  obj->data = arm;
  this->mapping.imported_objects.add(obj);
  if (!node->is_root) {
    this->mapping.el_to_object.add(&node->element, obj);
    if (this->params.use_custom_props) {
      read_custom_properties(node->props, obj->id, this->params.props_enum_as_string);
    }
    node_matrix_to_obj(node, obj, this->mapping);

    /* Record world to fbx node matrix for the armature object. */
    ufbx_matrix world_to_arm = ufbx_matrix_invert(&node->node_to_world);
    this->mapping.armature_world_to_arm_node_matrix.add(obj, world_to_arm);

    /* Record world to posed root node matrix. */
    if (node->bind_pose && node->bind_pose->is_bind_pose) {
      for (const ufbx_bone_pose &pose : node->bind_pose->bone_poses) {
        if (pose.bone_node == node) {
          world_to_arm = ufbx_matrix_invert(&pose.bone_to_world);
          break;
        }
      }
    }
    this->mapping.armature_world_to_arm_pose_matrix.add(obj, world_to_arm);
  }
  else {
    /* For armatures created at root, make them have the same rotation/scale
     * as done by ufbx for all regular nodes. */
    ufbx_matrix_to_obj(this->mapping.global_conv_matrix, obj);
    ufbx_matrix world_to_arm = ufbx_matrix_invert(&this->mapping.global_conv_matrix);
    this->mapping.armature_world_to_arm_pose_matrix.add(obj, world_to_arm);
    this->mapping.armature_world_to_arm_node_matrix.add(obj, world_to_arm);
  }
  return obj;
}

void ArmatureImportContext::create_armature_bones(const ufbx_node *node,
                                                  Object *arm_obj,
                                                  const Set<const ufbx_node *> &bone_nodes,
                                                  EditBone *parent_bone,
                                                  const ufbx_matrix &parent_mtx,
                                                  const ufbx_matrix &world_to_arm,
                                                  const float parent_bone_size)
{
  BLI_assert(node != nullptr && !node->is_root);
  bArmature *arm = static_cast<bArmature *>(arm_obj->data);

  /* Create an EditBone. */
  EditBone *bone = ED_armature_ebone_add(arm, get_fbx_name(node->name, "Bone"));
  this->mapping.node_to_name.add(node, bone->name);
  this->mapping.node_is_blender_bone.add(node);
  this->mapping.bone_to_armature.add(node, arm_obj);
  bone->flag |= BONE_SELECTED;
  bone->parent = parent_bone;
  if (node->inherit_mode == UFBX_INHERIT_MODE_IGNORE_PARENT_SCALE) {
    bone->inherit_scale_mode = BONE_INHERIT_SCALE_NONE;
  }
#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file,
          "create BONE %s (parent %s) parent_mtx:\n",
          node->name.data,
          parent_bone ? parent_bone->name : "");
  print_matrix(parent_mtx);
#endif

  ufbx_matrix bone_mtx = this->mapping.get_node_bind_matrix(node);
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
    if (!bone_nodes.contains(fchild)) {
      continue;
    }

    /* Estimate child position from local transform, but if the child
     * is skinned/posed then use the posed transform instead. */
    ufbx_vec3 pos = fchild->local_transform.translation;
    if (this->mapping.bone_to_bind_matrix.contains(fchild)) {
      ufbx_matrix local_mtx = this->mapping.calc_local_bind_matrix(fchild, world_to_arm);
      pos = local_mtx.cols[3];
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
    if (!this->mapping.bone_to_bind_matrix.contains(node)) {
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
  this->mapping.bone_to_length.add(node, bone_size);

  bone->tail[0] = 0.0f;
  bone->tail[1] = bone_size;
  bone->tail[2] = 0.0f;
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
      ufbx_vec3 par_tail;
      par_tail.x = 0;
      par_tail.y = parent_bone_size;
      par_tail.z = 0;
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
    if (!bone_nodes.contains(fchild)) {
      continue;
    }

    bool skip_child = false;
    if (this->params.ignore_leaf_bones) {
      if (node->children.count == 1 && fchild->children.count == 0 &&
          !mapping.bone_is_skinned.contains(fchild))
      {
        skip_child = true;
        /* We are skipping this bone, but still record it --
         * so that later code does not try to create an empty for it. */
        this->mapping.node_is_blender_bone.add(fchild);
      }
    }

    if (!skip_child) {
      create_armature_bones(fchild, arm_obj, bone_nodes, bone, bone_mtx, world_to_arm, bone_size);
    }
  }
}

/* Need to create armature if we are root bone, or any child is a non-root bone. */
static bool need_create_armature_for_node(const ufbx_node *node)
{
  if (node->bone && node->bone->is_root) {
    return true;
  }
  for (const ufbx_node *fchild : node->children) {
    if (fchild->bone && !fchild->bone->is_root) {
      return true;
    }
  }
  return false;
}

static void find_bones(const ufbx_node *node, Set<const ufbx_node *> &r_bones)
{
  if (node->bone != nullptr) {
    r_bones.add(node);
  }
  for (const ufbx_node *child : node->children) {
    find_bones(child, r_bones);
  }
}

static void find_fake_bones(const ufbx_node *root_node,
                            const Set<const ufbx_node *> &bones,
                            Set<const ufbx_node *> &r_fake_bones)
{
  for (const ufbx_node *bone_node : bones) {
    const ufbx_node *node = bone_node->parent;
    while (!ELEM(node, nullptr, root_node)) {
      if (node->bone == nullptr) {
        r_fake_bones.add(node);
      }
      node = node->parent;
    }
  }
}

static Set<const ufbx_node *> find_all_bones(const ufbx_node *root_node)
{
  /* Find regular FBX bones nodes anywhere under our root armature node. */
  Set<const ufbx_node *> bones;
  find_bones(root_node, bones);

  /* There might be non-bone nodes in between, e.g. FBX structure being like:
   * BoneA -> MeshB -> BoneC -> MeshD. Blender Armature can only contain
   * bones, so in this case "MeshB" has to have a bone created for it as well.
   * "Fake bones" are any non-bone FBX nodes in between root armature node
   * and the actual bone node. */
  Set<const ufbx_node *> fake_bones;
  find_fake_bones(root_node, bones, fake_bones);
  for (const ufbx_node *b : fake_bones) {
    bones.add(b);
  }
  return bones;
}

void ArmatureImportContext::find_armatures(const ufbx_node *node)
{
  const bool needs_arm = need_create_armature_for_node(node);
  if (needs_arm) {
    /* Create armature. */
    Object *arm_obj = this->create_armature_for_node(node);
    ufbx_matrix world_to_arm = this->mapping.armature_world_to_arm_pose_matrix.lookup_default(
        arm_obj, ufbx_identity_matrix);

    Set<const ufbx_node *> bone_nodes = find_all_bones(node);

    /* Create bones in edit mode. */
    bArmature *arm = static_cast<bArmature *>(arm_obj->data);
    ED_armature_to_edit(arm);
    this->mapping.node_to_name.add(node, BKE_id_name(arm_obj->id));
    for (const ufbx_node *fchild : node->children) {
      if (bone_nodes.contains(fchild)) {
        create_armature_bones(
            fchild, arm_obj, bone_nodes, nullptr, ufbx_identity_matrix, world_to_arm, 1.0f);
      }
    }

    ED_armature_from_edit(&this->bmain, arm);
    ED_armature_edit_free(arm);

    /* Setup pose on the object, and custom properties on the bone pose channels. */
    for (const ufbx_node *fbone : bone_nodes) {
      if (!this->mapping.node_is_blender_bone.contains(fbone)) {
        continue; /* Blender bone was not created for it (e.g. root bone in some cases). */
      }
      bPoseChannel *pchan = BKE_pose_channel_find_name(
          arm_obj->pose, this->mapping.node_to_name.lookup_default(fbone, "").c_str());
      if (pchan == nullptr) {
        continue;
      }
      read_custom_properties(fbone->props, *pchan, this->params.props_enum_as_string);

      /* For bones that have rest/bind information, put their current transform into
       * the current pose. */
      if (this->mapping.bone_to_bind_matrix.contains(fbone)) {
        ufbx_matrix bind_local_mtx = this->mapping.calc_local_bind_matrix(fbone, world_to_arm);
        ufbx_matrix bind_local_mtx_inv = ufbx_matrix_invert(&bind_local_mtx);
        ufbx_transform xform = fbone->local_transform;
        if (fbone->node_depth <= 1) {
          ufbx_matrix matrix = ufbx_matrix_mul(&world_to_arm, &fbone->node_to_world);
          xform = ufbx_matrix_to_transform(&matrix);
        }
        ufbx_matrix pose_mtx = calc_bone_pose_matrix(xform, *fbone, bind_local_mtx_inv);

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

  /* Recurse into children that have not been turned into bones yet. */
  for (const ufbx_node *fchild : node->children) {
    if (!this->mapping.node_is_blender_bone.contains(fchild)) {
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
      this->mapping.bone_is_skinned.add(fbone->bone_node);
#ifdef FBX_DEBUG_PRINT
      fprintf(g_debug_file, "bone SKIN matrix %s\n", fbone->bone_node->name.data);
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
  context.find_armatures(fbx.root_node);
}

}  // namespace blender::io::fbx
