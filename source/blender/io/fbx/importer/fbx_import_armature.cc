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
#include "BLI_stack.hh"

#include "DNA_object_types.h"

#include "ED_armature.hh"

#include "IO_fbx.hh"

#include "fbx_import_armature.hh"

namespace blender::io::fbx {

static constexpr float auto_orient_min_length = 1.0e-5f;

struct ArmatureImportContext {
  Main &bmain;
  const ufbx_scene &fbx;
  const FBXImportParams &params;
  FbxElementMapping &mapping;

  /* Bone lengths calculated by automatic bone orientation. */
  Map<const ufbx_node *, float> auto_bone_length;
  /* Animated transform properties (translation, rotation, scale) of each node, in anim layer
   * order. Used to check whether a bone has translation animation. */
  struct NodeTransformAnim {
    const ufbx_anim_layer *layer;
    const ufbx_anim_prop *prop;
  };
  Map<const ufbx_node *, Vector<NodeTransformAnim>> node_transform_anims;

  ArmatureImportContext(Main &main,
                        const ufbx_scene &fbx,
                        const FBXImportParams &params,
                        FbxElementMapping &mapping)
      : bmain(main), fbx(fbx), params(params), mapping(mapping)
  {
  }

  float3 calc_child_bone_local_pos(const ufbx_node *child, const ufbx_matrix &world_to_arm) const;
  float3 calc_bone_incoming_dir(const ufbx_node *node, const ufbx_matrix &world_to_arm) const;
  void calc_bone_orientations(const ufbx_node *arm_node,
                              const Set<const ufbx_node *> &bone_nodes,
                              const ufbx_matrix &world_to_arm);
  void gather_node_transform_anims();
  bool is_translation_animated(const ufbx_node *node) const;
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

float3 ArmatureImportContext::calc_child_bone_local_pos(const ufbx_node *child,
                                                        const ufbx_matrix &world_to_arm) const
{
  /* Estimate child position from local transform. If the child
   * is skinned/posed, then use the posed transform. */
  ufbx_vec3 pos = child->local_transform.translation;
  if (this->mapping.bone_to_bind_matrix.contains(child)) {
    ufbx_matrix local_mtx = this->mapping.calc_local_bind_matrix(child, world_to_arm);
    pos = local_mtx.cols[3];
  }
  return float3(pos.x, pos.y, pos.z);
}

float3 ArmatureImportContext::calc_bone_incoming_dir(const ufbx_node *node,
                                                     const ufbx_matrix &world_to_arm) const
{
  /* Direction from parent joint towards this joint, in the local space of this joint. */
  ufbx_matrix local_mtx;
  if (this->mapping.bone_to_bind_matrix.contains(node)) {
    local_mtx = this->mapping.calc_local_bind_matrix(node, world_to_arm);
  }
  else {
    local_mtx = ufbx_transform_to_matrix(&node->local_transform);
  }
  const ufbx_vec3 &pos = local_mtx.cols[3];
  if (math::length(float3(pos.x, pos.y, pos.z)) < auto_orient_min_length) {
    /* Coincident joints have no sensible incoming direction. */
    return float3(0.0f);
  }
  const ufbx_matrix local_inv = ufbx_matrix_invert(&local_mtx);
  const float3 dir = -float3(local_inv.cols[3].x, local_inv.cols[3].y, local_inv.cols[3].z);
  return math::normalize(dir);
}

/* Build bone correction matrix that rotates bone Y axis to point along the given
 * axis (0=X, 1=Y, 2=Z) with the given sign. Resulting matrix is rotation only,
 * by multiples of 90 degrees. */
static ufbx_matrix make_bone_correction_matrix(int axis, float sign)
{
  float3 new_x, new_y(0.0f), new_z;
  new_y[axis] = sign;
  if (axis != 2) {
    new_z = float3(0.0f, 0.0f, 1.0f);
    new_x = math::cross(new_y, new_z);
  }
  else {
    new_x = float3(1.0f, 0.0f, 0.0f);
    new_z = math::cross(new_x, new_y);
  }
  ufbx_matrix mtx = ufbx_identity_matrix;
  for (int i = 0; i < 3; i++) {
    mtx.cols[0].v[i] = new_x[i];
    mtx.cols[1].v[i] = new_y[i];
    mtx.cols[2].v[i] = new_z[i];
  }
  return mtx;
}

/* Automatic bone orientation logic.
 *
 * Blender bones always point along their Y axis, and are visualized as such. But in FBX, joints
 * do not have a "direction", rather just parent-child hierarchy. FBX files produced by
 * other applications often have for example X axis pointing along the bone direction.
 *
 * - For each bone, figure out local direction it "should" point to: towards the child joint
 *   (with multiple children, the one that best continues the parent->bone direction), or for
 *   leaf bones, continuing the parent->bone direction.
 * - Pick one axis (X/Y/Z, ignoring the sign) for the whole armature, by majority vote among
 *   bones that have a single child.
 * - Pick the sign per bone, since with mirrored skeletons some bones might point along
 *   the +axis, and others point along the -axis. If the bone direction is not clearly along the
 *   axis, use the sign of the parent bone.
 * - Bones with a child far away from the armature axis (most often happens with root bones or
 *   various helper bones) use the axis closest to the child direction instead.
 * - If the skeleton already follows the Blender convention (+Y axis), do not change anything.
 *
 * The correction is always rotation by multiples of 90 degrees, so that non-uniform bone scaling
 * stays representable. */
void ArmatureImportContext::calc_bone_orientations(const ufbx_node *arm_node,
                                                   const Set<const ufbx_node *> &bone_nodes,
                                                   const ufbx_matrix &world_to_arm)
{
  struct BoneDir {
    float3 dir = float3(0.0f); /* Normalized, or zero if unknown. */
    int child_count = 0;
  };
  Map<const ufbx_node *, BoneDir> bone_dirs;

  /* Bone lengths to use, if any correction ends up being done. */
  Map<const ufbx_node *, float> bone_lengths;

  for (const ufbx_node *node : bone_nodes) {
    BoneDir info;
    const float3 incoming = this->calc_bone_incoming_dir(node, world_to_arm);
    float best_score = 0.0f;
    float3 best_pos(0.0f);
    bool has_coincident_children = false;
    for (const ufbx_node *fchild : node->children) {
      if (!bone_nodes.contains(fchild)) {
        continue;
      }
      const float3 pos = this->calc_child_bone_local_pos(fchild, world_to_arm);
      const float len = math::length(pos);
      if (len < auto_orient_min_length) {
        has_coincident_children = true;
        continue;
      }
      info.child_count++;

      /* Prefer the child that best continues the parent->bone direction. If that is not known,
       * then use the farthest child. */
      const float score = math::is_zero(incoming) ? len : math::dot(pos / len, incoming);
      if (info.child_count == 1 || score > best_score) {
        best_score = score;
        best_pos = pos;
      }
    }
    if (info.child_count > 0) {
      info.dir = math::normalize(best_pos);
      bone_lengths.add(node, math::length(best_pos));
    }
    else {
      info.dir = incoming;
      if (has_coincident_children) {
        /* All children are at the bone head, minimum bone length clamp will handle this. */
        bone_lengths.add(node, 0.0f);
      }
    }
    bone_dirs.add(node, info);
  }

  /* Vote on the axis. Prefer bones with a single child, since their direction is unambiguous;
   * use all bones with children if there are no such bones. */
  int votes[3] = {0, 0, 0};
  for (const bool single_only : {true, false}) {
    for (const BoneDir &info : bone_dirs.values()) {
      if (single_only ? info.child_count != 1 : info.child_count == 0) {
        continue;
      }
      votes[math::dominant_axis(info.dir)]++;
    }
    if (votes[0] + votes[1] + votes[2] > 0) {
      break;
    }
  }

  /* For ties, prefer Y (no change compared to Blender), then X. */
  int axis = 1;
  for (const int candidate : {0, 2}) {
    if (votes[candidate] > votes[axis]) {
      axis = candidate;
    }
  }

#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file,
          "auto bone orientation for %s: votes X %i Y %i Z %i, axis %i\n",
          arm_node->name.data,
          votes[0],
          votes[1],
          votes[2],
          axis);
#endif

  /* Pick sign for each bone, going down the hierarchy from the root bones. */
  Stack<std::pair<const ufbx_node *, float>> stack;
  for (const ufbx_node *fchild : arm_node->children) {
    if (bone_nodes.contains(fchild)) {
      stack.push({fchild, 1.0f});
    }
  }
  /* Only switch sign from parent when bone direction is within 60 degrees of the axis. */
  const float min_axis_cos = 0.5f;
  bool any_correction = false;
  Vector<const ufbx_node *> outlier_bones;
  while (!stack.is_empty()) {
    const auto [node, parent_sign] = stack.pop();
    const BoneDir &info = bone_dirs.lookup(node);
    const float dir_along_axis = info.dir[axis];
    float sign = parent_sign;
    if (math::abs(dir_along_axis) >= min_axis_cos) {
      sign = dir_along_axis > 0.0f ? 1.0f : -1.0f;
    }
    else if (info.child_count > 0) {
      /* Outlier, will probably get its own axis below. */
      outlier_bones.append(node);
    }
    if (axis != 1 || sign != 1.0f) {
      this->mapping.bone_to_correction.add(node, make_bone_correction_matrix(axis, sign));
      any_correction = true;
    }
    for (const ufbx_node *fchild : node->children) {
      if (bone_nodes.contains(fchild)) {
        stack.push({fchild, sign});
      }
    }
  }

  /* Leave the skeleton as-is if there is no correction needed. */
  if (!any_correction) {
    return;
  }

  /* Outlier bones that have children use the axis closest to their child direction. */
  for (const ufbx_node *node : outlier_bones) {
    const float3 dir = bone_dirs.lookup(node).dir;
    const int bone_axis = math::dominant_axis(dir);
    const float bone_sign = dir[bone_axis] > 0.0f ? 1.0f : -1.0f;
    if (bone_axis == 1 && bone_sign == 1.0f) {
      this->mapping.bone_to_correction.remove(node);
    }
    else {
      this->mapping.bone_to_correction.add_overwrite(
          node, make_bone_correction_matrix(bone_axis, bone_sign));
    }
  }

  for (const auto item : bone_lengths.items()) {
    this->auto_bone_length.add(item.key, item.value);
  }
}

void ArmatureImportContext::gather_node_transform_anims()
{
  for (const ufbx_anim_layer *layer : this->fbx.anim_layers) {
    for (const ufbx_anim_prop &prop : layer->anim_props) {
      if (prop.element->type != UFBX_ELEMENT_NODE || prop.anim_value == nullptr) {
        continue;
      }
      if (!STREQ(prop.prop_name.data, UFBX_Lcl_Translation) &&
          !STREQ(prop.prop_name.data, UFBX_Lcl_Rotation) &&
          !STREQ(prop.prop_name.data, UFBX_Lcl_Scaling))
      {
        continue;
      }
      const ufbx_node *node = ufbx_as_node(prop.element);
      this->node_transform_anims.lookup_or_add_default(node).append({layer, &prop});
    }
  }
}

bool ArmatureImportContext::is_translation_animated(const ufbx_node *node) const
{
  const Vector<NodeTransformAnim> *anims = this->node_transform_anims.lookup_ptr(node);
  if (anims == nullptr) {
    return false;
  }

  /* It is not enough to check just presence of translation animation, because
   * translation can change due to rotation/scale around a pivot too. Evaluate the transform
   * the same way as animation import does. */
  const ufbx_vec3 &cur_pos = node->local_transform.translation;
  Set<double> key_times;
  for (int64_t i = 0; i < anims->size();) {
    const ufbx_anim_layer *layer = (*anims)[i].layer;
    key_times.clear();
    for (; i < anims->size() && (*anims)[i].layer == layer; i++) {
      for (const ufbx_anim_curve *curve : (*anims)[i].prop->anim_value->curves) {
        if (curve == nullptr) {
          continue;
        }
        for (const ufbx_keyframe &key : curve->keyframes) {
          key_times.add(key.time);
        }
      }
    }

    for (const double time : key_times) {
      const ufbx_transform xform = ufbx_evaluate_transform(layer->anim, node, time);
      for (int axis = 0; axis < 3; axis++) {
        const ufbx_real eps = 1.0e-4 * math::max(ufbx_real(1.0), math::abs(cur_pos.v[axis]));
        if (math::abs(xform.translation.v[axis] - cur_pos.v[axis]) > eps) {
          return true;
        }
      }
    }
  }
  return false;
}

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
  obj->data = id_cast<ID *>(arm);
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
  bArmature *arm = id_cast<bArmature *>(arm_obj->data);

  /* Create an EditBone. */
  std::string name;
  if (node->is_geometry_transform_helper) {
    /* Name geometry transform adjustment helpers with parent name and _GeomAdjust suffix. */
    name = get_fbx_name(node->parent->name, "Bone") + std::string("_GeomAdjust");
  }
  else {
    name = get_fbx_name(node->name, "Bone");
  }
  EditBone *bone = ED_armature_ebone_add(arm, name.c_str());
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
    bone_size += math::length(this->calc_child_bone_local_pos(fchild, world_to_arm));
    child_bone_count++;
  }
  if (const float *auto_length = this->auto_bone_length.lookup_ptr(node)) {
    bone_size = *auto_length;
  }
  else if (child_bone_count > 0) {
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
  /* Set bone matrix. Note: the uncorrected `bone_mtx` is what gets passed to the children. */
  const ufbx_matrix *correction = this->mapping.find_bone_correction(node);
  float bone_matrix[4][4];
  if (correction != nullptr) {
    const ufbx_matrix corrected_mtx = ufbx_matrix_mul(&bone_mtx, correction);
    matrix_to_m44(corrected_mtx, bone_matrix);
  }
  else {
    matrix_to_m44(bone_mtx, bone_matrix);
  }
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
   * both rest pose and current pose.
   * Connected bones ignore their pose translation, so do not connect bones that have
   * translation animation. */
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
      if (const ufbx_matrix *par_correction = this->mapping.find_bone_correction(node->parent)) {
        par_tail = ufbx_transform_position(par_correction, par_tail);
      }
      ufbx_vec3 par_tail_cur_u = ufbx_transform_position(&node->parent->node_to_world, par_tail);
      float3 self_head_cur(self_head_cur_u.x, self_head_cur_u.y, self_head_cur_u.z);
      float3 par_tail_cur(par_tail_cur_u.x, par_tail_cur_u.y, par_tail_cur_u.z);
      float dist_sq_cur = math::distance_squared(self_head_cur, par_tail_cur);

      if (dist_sq_cur < connect_dist_sq && !this->is_translation_animated(node)) {
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

    if (this->params.auto_bone_orientation) {
      this->calc_bone_orientations(node, bone_nodes, world_to_arm);
    }

    /* Create bones in edit mode. */
    bArmature *arm = id_cast<bArmature *>(arm_obj->data);
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
        ufbx_matrix pose_mtx = calc_bone_pose_matrix(
            xform, *fbone, bind_local_mtx_inv, this->mapping.find_bone_correction(fbone));

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
  if (params.use_anim) {
    context.gather_node_transform_anims();
  }
  context.find_armatures(fbx.root_node);
}

}  // namespace blender::io::fbx
