/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include <algorithm>

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"

#include "BKE_action.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_id.hh"
#include "BKE_object_types.hh"

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_math_axis_angle.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "fbx_import_anim.hh"
#include "fbx_import_util.hh"

namespace blender::io::fbx {

static FCurve *create_fcurve(animrig::Channelbag &channelbag,
                             const animrig::FCurveDescriptor &descriptor,
                             int64_t key_count)
{
  FCurve *cu = channelbag.fcurve_create_unique(nullptr, descriptor);
  BLI_assert_msg(cu, "The same F-Curve is being created twice, this is unexpected.");
  BKE_fcurve_bezt_resize(cu, key_count);
  return cu;
}

static void set_curve_sample(FCurve *curve, int64_t key_index, float time, float value)
{
  BLI_assert(key_index >= 0 && key_index < curve->totvert);
  BezTriple &bez = curve->bezt[key_index];
  bez.vec[1][0] = time;
  bez.vec[1][1] = value;
  bez.ipo = BEZT_IPO_LIN;
  bez.f1 = bez.f2 = bez.f3 = SELECT;
  bez.h1 = bez.h2 = HD_AUTO_ANIM;
}

struct ElementAnimations {
  const ufbx_element *fbx_elem = nullptr;
  ID *target_id = nullptr;
  eRotationModes object_rotmode = ROT_MODE_QUAT;
  int64_t order = 0;
  const ufbx_anim_prop *prop_position = nullptr;
  const ufbx_anim_prop *prop_rotation = nullptr;
  const ufbx_anim_prop *prop_scale = nullptr;
  const ufbx_anim_prop *prop_blend_shape = nullptr;
  const ufbx_anim_prop *prop_focal_length = nullptr;
  const ufbx_anim_prop *prop_focus_dist = nullptr;
  const ufbx_anim_prop *prop_mat_diffuse = nullptr;
};

static Vector<ElementAnimations> gather_animated_properties(const FbxElementMapping &mapping,
                                                            const ufbx_anim_layer &flayer)
{
  int64_t order = 0;
  Map<const ufbx_element *, ElementAnimations> elem_map;
  for (const ufbx_anim_prop &fprop : flayer.anim_props) {
    if (fprop.anim_value->curves[0] == nullptr) {
      continue;
    }
    bool supported_prop = false;
    //@TODO: "Visibility"?
    const bool is_position = STREQ(fprop.prop_name.data, "Lcl Translation");
    const bool is_rotation = STREQ(fprop.prop_name.data, "Lcl Rotation");
    const bool is_scale = STREQ(fprop.prop_name.data, "Lcl Scaling");
    const bool is_blend_shape = STREQ(fprop.prop_name.data, "DeformPercent");
    const bool is_focal_length = STREQ(fprop.prop_name.data, "FocalLength");
    const bool is_focus_dist = STREQ(fprop.prop_name.data, "FocusDistance");
    const bool is_diffuse = STREQ(fprop.prop_name.data, "DiffuseColor");
    if (is_position || is_rotation || is_scale || is_blend_shape || is_focal_length ||
        is_focus_dist || is_diffuse)
    {
      supported_prop = true;
    }

    if (!supported_prop) {
      continue;
    }

    const bool is_anim_camera = is_focal_length || is_focus_dist;
    const bool is_anim_mat = is_diffuse;

    ID *target_id = nullptr;
    eRotationModes object_rotmode = ROT_MODE_QUAT;

    if (is_blend_shape) {
      /* Animating blend shape weight. */
      Key *target_key = mapping.el_to_shape_key.lookup_default(fprop.element, nullptr);
      if (target_key != nullptr) {
        target_id = &target_key->id;
      }
    }
    else if (is_anim_camera) {
      /* Animating camera property. */
      if (fprop.element->instances.count > 0) {
        Object *obj = mapping.el_to_object.lookup_default(&fprop.element->instances[0]->element,
                                                          nullptr);
        if (obj != nullptr && obj->type == OB_CAMERA) {
          target_id = (ID *)obj->data;
          object_rotmode = static_cast<eRotationModes>(obj->rotmode);
        }
      }
    }
    else if (is_anim_mat) {
      /* Animating material property. */
      Material *mat = mapping.mat_to_material.lookup_default((ufbx_material *)fprop.element,
                                                             nullptr);
      if (mat != nullptr) {
        target_id = (ID *)mat;
      }
    }
    else {
      /* Animating Bone/Armature/Object property. */
      const ufbx_node *fnode = ufbx_as_node(fprop.element);
      Object *obj = nullptr;
      if (fnode) {
        obj = mapping.bone_to_armature.lookup_default(fnode, nullptr);
      }
      if (obj == nullptr) {
        obj = mapping.el_to_object.lookup_default(fprop.element, nullptr);
      }
      if (obj == nullptr) {
        continue;
      }
      /* Ignore animation of rigged meshes (very hard to handle; matches behavior of python fbx
       * importer). */
      if (obj->type == OB_MESH && obj->parent && obj->parent->type == OB_ARMATURE) {
        continue;
      }
      target_id = &obj->id;
      object_rotmode = static_cast<eRotationModes>(obj->rotmode);
    }

    if (target_id == nullptr) {
      continue;
    }

    ElementAnimations &anims = elem_map.lookup_or_add_default(fprop.element);
    anims.fbx_elem = fprop.element;
    anims.order = order++;
    anims.target_id = target_id;
    anims.object_rotmode = object_rotmode;

    if (is_position) {
      anims.prop_position = &fprop;
    }
    if (is_rotation) {
      anims.prop_rotation = &fprop;
    }
    if (is_scale) {
      anims.prop_scale = &fprop;
    }
    if (is_blend_shape) {
      anims.prop_blend_shape = &fprop;
    }
    if (is_focal_length) {
      anims.prop_focal_length = &fprop;
    }
    if (is_focus_dist) {
      anims.prop_focus_dist = &fprop;
    }
    if (is_diffuse) {
      anims.prop_mat_diffuse = &fprop;
    }
  }

  /* Sort returned result in the original fbx file order. */
  Vector<ElementAnimations> animations(elem_map.values().begin(), elem_map.values().end());
  std::sort(
      animations.begin(),
      animations.end(),
      [](const ElementAnimations &a, const ElementAnimations &b) { return a.order < b.order; });
  return animations;
}

static void finalize_curve(FCurve *cu)
{
  if (cu != nullptr) {
    BKE_fcurve_handles_recalc(cu);
  }
}

static void create_transform_curve_desc(const FbxElementMapping &mapping,
                                        const ElementAnimations &anim,
                                        LinearAllocator<> &curve_name_alloc,
                                        Vector<animrig::FCurveDescriptor> &r_curve_desc)
{
  /* For animated bones, prepend bone path to animation curve path. */
  std::string rna_prefix;
  std::string group_name_str = get_fbx_name(anim.fbx_elem->name);
  const ufbx_node *fnode = ufbx_as_node(anim.fbx_elem);
  const bool is_bone = mapping.node_is_blender_bone.contains(fnode);
  if (is_bone) {
    group_name_str = mapping.node_to_name.lookup_default(fnode, "");
    rna_prefix = std::string("pose.bones[\"") + group_name_str + "\"].";
  }

  StringRefNull group_name = curve_name_alloc.copy_string(group_name_str);

  StringRefNull rna_position = curve_name_alloc.copy_string(rna_prefix + "location");

  StringRefNull rna_rotation;
  int rot_channels = 3;
  /* Bones are created with quaternion rotation. */
  eRotationModes rot_mode = is_bone ? ROT_MODE_QUAT : anim.object_rotmode;
  switch (rot_mode) {
    case ROT_MODE_QUAT:
      rna_rotation = curve_name_alloc.copy_string(rna_prefix + "rotation_quaternion");
      rot_channels = 4;
      break;
    case ROT_MODE_AXISANGLE:
      rna_rotation = curve_name_alloc.copy_string(rna_prefix + "rotation_axis_angle");
      rot_channels = 4;
      break;
    default:
      rna_rotation = curve_name_alloc.copy_string(rna_prefix + "rotation_euler");
      rot_channels = 3;
      break;
  }

  StringRefNull rna_scale = curve_name_alloc.copy_string(rna_prefix + "scale");

  /* Fill the f-curve descriptors. */
  for (int i = 0; i < 3; i++) {
    r_curve_desc.append({rna_position, i, {}, {}, group_name});
  }
  for (int i = 0; i < rot_channels; i++) {
    r_curve_desc.append({rna_rotation, i, {}, {}, group_name});
  }
  for (int i = 0; i < 3; i++) {
    r_curve_desc.append({rna_scale, i, {}, {}, group_name});
  }
}

static void create_transform_curve_data(const FbxElementMapping &mapping,
                                        const ufbx_anim *fbx_anim,
                                        const ElementAnimations &anim,
                                        const double fps,
                                        const float anim_offset,
                                        FCurve **curves)
{
  const ufbx_node *fnode = ufbx_as_node(anim.fbx_elem);
  ufbx_matrix bone_xform = ufbx_identity_matrix;
  const bool is_bone = mapping.node_is_blender_bone.contains(fnode);
  if (is_bone) {
    /* Bone transform curves need to be transformed to the bind transform
     * in joint-local space:
     * - Calculate local space bind matrix: inv(parent_bind) * bind
     * - Invert the result; this will be used to transform loc/rot/scale curves. */

    const bool bone_at_scene_root = fnode->node_depth <= 1;
    ufbx_matrix world_to_arm = ufbx_identity_matrix;
    if (!bone_at_scene_root) {
      Object *arm_obj = mapping.bone_to_armature.lookup_default(fnode, nullptr);
      if (arm_obj != nullptr) {
        world_to_arm = mapping.armature_world_to_arm_pose_matrix.lookup_default(
            arm_obj, ufbx_identity_matrix);
      }
    }

    bone_xform = mapping.calc_local_bind_matrix(fnode, world_to_arm);
    bone_xform = ufbx_matrix_invert(&bone_xform);
  }

  int rot_channels = 3;
  /* Bones are created with quaternion rotation. */
  eRotationModes rot_mode = is_bone ? ROT_MODE_QUAT : anim.object_rotmode;
  switch (rot_mode) {
    case ROT_MODE_QUAT:
      rot_channels = 4;
      break;
    case ROT_MODE_AXISANGLE:
      rot_channels = 4;
      break;
    default:
      rot_channels = 3;
      break;
  }

  /* Note: Python importer was always creating all pos/rot/scale curves: "due to all FBX
   * transform magic, we need to add curves for whole loc/rot/scale in any case".
   *
   * Also, we create a full transform keyframe at any point where input pos/rot/scale curves have
   * a keyframe. It should not be needed if we fully imported curves with all their proper
   * handles, but again currently this is to match Python importer behavior. */
  const ufbx_anim_curve *input_curves[9] = {};
  if (anim.prop_position) {
    input_curves[0] = anim.prop_position->anim_value->curves[0];
    input_curves[1] = anim.prop_position->anim_value->curves[1];
    input_curves[2] = anim.prop_position->anim_value->curves[2];
  }
  if (anim.prop_rotation) {
    input_curves[3] = anim.prop_rotation->anim_value->curves[0];
    input_curves[4] = anim.prop_rotation->anim_value->curves[1];
    input_curves[5] = anim.prop_rotation->anim_value->curves[2];
  }
  if (anim.prop_scale) {
    input_curves[6] = anim.prop_scale->anim_value->curves[0];
    input_curves[7] = anim.prop_scale->anim_value->curves[1];
    input_curves[8] = anim.prop_scale->anim_value->curves[2];
  }

  /* Figure out timestamps of where any of input curves have a keyframe. */
  Set<double> unique_key_times;
  for (int i = 0; i < 9; i++) {
    if (input_curves[i] != nullptr) {
      for (const ufbx_keyframe &key : input_curves[i]->keyframes) {
        if (key.interpolation == UFBX_INTERPOLATION_CUBIC) {
          /* Hack: force cubic keyframes to be linear, to match Python importer behavior. */
          const_cast<ufbx_keyframe &>(key).interpolation = UFBX_INTERPOLATION_LINEAR;
        }
        unique_key_times.add(key.time);
      }
    }
  }
  Vector<double> sorted_key_times(unique_key_times.begin(), unique_key_times.end());
  std::sort(sorted_key_times.begin(), sorted_key_times.end());

  int64_t pos_index = 0;
  int64_t rot_index = pos_index + 3;
  int64_t scale_index = rot_index + rot_channels;
  int64_t tot_curves = scale_index + 3;
  for (int64_t i = 0; i < tot_curves; i++) {
    BLI_assert_msg(curves[i], "fbx: animation curve was not created successfully");
    BKE_fcurve_bezt_resize(curves[i], sorted_key_times.size());
  }

  /* Evaluate transforms at all the key times. */
  math::Quaternion quat_prev = math::Quaternion::identity();
  for (int64_t i = 0; i < sorted_key_times.size(); i++) {
    double t = sorted_key_times[i];
    float tf = float(t * fps + anim_offset);
    ufbx_transform xform = ufbx_evaluate_transform(fbx_anim, fnode, t);

    if (is_bone) {
      ufbx_matrix matrix = calc_bone_pose_matrix(xform, *fnode, bone_xform);
      xform = ufbx_matrix_to_transform(&matrix);
    }

    set_curve_sample(curves[pos_index + 0], i, tf, float(xform.translation.x));
    set_curve_sample(curves[pos_index + 1], i, tf, float(xform.translation.y));
    set_curve_sample(curves[pos_index + 2], i, tf, float(xform.translation.z));

    math::Quaternion quat(xform.rotation.w, xform.rotation.x, xform.rotation.y, xform.rotation.z);
    switch (rot_mode) {
      case ROT_MODE_QUAT:
        /* Ensure shortest interpolation path between consecutive quaternions. */
        if (i != 0 && math::dot(quat, quat_prev) < 0.0f) {
          quat = -quat;
        }
        quat_prev = quat;
        set_curve_sample(curves[rot_index + 0], i, tf, quat.w);
        set_curve_sample(curves[rot_index + 1], i, tf, quat.x);
        set_curve_sample(curves[rot_index + 2], i, tf, quat.y);
        set_curve_sample(curves[rot_index + 3], i, tf, quat.z);
        break;
      case ROT_MODE_AXISANGLE: {
        const math::AxisAngle axis_angle = math::to_axis_angle(quat);
        set_curve_sample(curves[rot_index + 0], i, tf, axis_angle.angle().radian());
        set_curve_sample(curves[rot_index + 1], i, tf, axis_angle.axis().x);
        set_curve_sample(curves[rot_index + 2], i, tf, axis_angle.axis().y);
        set_curve_sample(curves[rot_index + 3], i, tf, axis_angle.axis().z);
      } break;
      default: {
        math::EulerXYZ euler = math::to_euler(quat);
        set_curve_sample(curves[rot_index + 0], i, tf, euler.x().radian());
        set_curve_sample(curves[rot_index + 1], i, tf, euler.y().radian());
        set_curve_sample(curves[rot_index + 2], i, tf, euler.z().radian());
      } break;
    }

    set_curve_sample(curves[scale_index + 0], i, tf, float(xform.scale.x));
    set_curve_sample(curves[scale_index + 1], i, tf, float(xform.scale.y));
    set_curve_sample(curves[scale_index + 2], i, tf, float(xform.scale.z));
  }
}

static void create_camera_curves(const ufbx_metadata &metadata,
                                 const ElementAnimations &anim,
                                 animrig::Channelbag &channelbag,
                                 const double fps,
                                 const float anim_offset)
{
  if (anim.target_id == nullptr || GS(anim.target_id->name) != ID_CA) {
    return;
  }

  if (anim.prop_focal_length != nullptr) {
    const ufbx_anim_curve *input_curve = anim.prop_focal_length->anim_value->curves[0];
    FCurve *curve = create_fcurve(channelbag, {"lens", 0}, input_curve->keyframes.count);
    for (int i = 0; i < input_curve->keyframes.count; i++) {
      const ufbx_keyframe &fkey = input_curve->keyframes[i];
      float tf = float(fkey.time * fps + anim_offset);
      float val = float(fkey.value);
      set_curve_sample(curve, i, tf, val);
    }
    finalize_curve(curve);
  }

  if (anim.prop_focus_dist != nullptr) {
    const ufbx_anim_curve *input_curve = anim.prop_focus_dist->anim_value->curves[0];
    FCurve *curve = create_fcurve(
        channelbag, {"dof.focus_distance", 0}, input_curve->keyframes.count);
    for (int i = 0; i < input_curve->keyframes.count; i++) {
      const ufbx_keyframe &fkey = input_curve->keyframes[i];
      float tf = float(fkey.time * fps + anim_offset);
      /* Animation curves containing camera focus distance have values multiplied by 1000.0 */
      float val = float(fkey.value / 1000.0 * metadata.geometry_scale * metadata.root_scale);
      set_curve_sample(curve, i, tf, val);
    }
    finalize_curve(curve);
  }
}

static void create_material_curves(const ElementAnimations &anim,
                                   bAction *action,
                                   animrig::Channelbag &channelbag,
                                   const double fps,
                                   const float anim_offset)
{
  if (anim.target_id == nullptr || GS(anim.target_id->name) != ID_MA) {
    return;
  }

  const char *rna_path_1 = "diffuse_color";
  const char *rna_path_2 = "nodes[\"Principled BSDF\"].inputs[0].default_value";

  /* Also create animation curves for the node tree diffuse color input. */
  Material *target_mat = (Material *)anim.target_id;
  ID *target_ntree = (ID *)target_mat->nodetree;
  animrig::Action &act = action->wrap();
  const animrig::Slot *slot = animrig::assign_action_ensure_slot_for_keying(act, *target_ntree);
  BLI_assert(slot != nullptr);
  UNUSED_VARS_NDEBUG(slot);
  animrig::Channelbag &chbag_node = animrig::action_channelbag_ensure(*action, *target_ntree);

  if (anim.prop_mat_diffuse != nullptr) {
    for (int ch = 0; ch < 3; ch++) {
      const ufbx_anim_curve *input_curve = anim.prop_mat_diffuse->anim_value->curves[ch];
      FCurve *curve_1 = create_fcurve(channelbag, {rna_path_1, ch}, input_curve->keyframes.count);
      FCurve *curve_2 = create_fcurve(chbag_node, {rna_path_2, ch}, input_curve->keyframes.count);
      for (int i = 0; i < input_curve->keyframes.count; i++) {
        const ufbx_keyframe &fkey = input_curve->keyframes[i];
        float tf = float(fkey.time * fps + anim_offset);
        float val = float(fkey.value);
        set_curve_sample(curve_1, i, tf, val);
        set_curve_sample(curve_2, i, tf, val);
      }
      finalize_curve(curve_1);
      finalize_curve(curve_2);
    }
  }
}

static void create_blend_shape_curves(const ElementAnimations &anim,
                                      animrig::Channelbag &channelbag,
                                      const double fps,
                                      const float anim_offset)
{
  const ufbx_blend_channel *fchan = ufbx_as_blend_channel(anim.prop_blend_shape->element);
  BLI_assert(fchan != nullptr);
  std::string rna_path = std::string("key_blocks[\"") + fchan->target_shape->name.data +
                         "\"].value";
  const ufbx_anim_curve *input_curve = anim.prop_blend_shape->anim_value->curves[0];
  FCurve *curve = create_fcurve(channelbag, {rna_path, 0}, input_curve->keyframes.count);
  for (int i = 0; i < input_curve->keyframes.count; i++) {
    const ufbx_keyframe &fkey = input_curve->keyframes[i];
    double t = fkey.time;
    float tf = float(t * fps + anim_offset);
    float val = float(fkey.value / 100.0); /* FBX shape weights are 0..100 range. */
    set_curve_sample(curve, i, tf, val);
  }
  finalize_curve(curve);
}

void import_animations(Main &bmain,
                       const ufbx_scene &fbx,
                       const FbxElementMapping &mapping,
                       const double fps,
                       const float anim_offset)
{
  /* Note: mixing is completely ignored for now, each layer results in an independent set of
   * actions. */
  for (const ufbx_anim_stack *fstack : fbx.anim_stacks) {
    for (const ufbx_anim_layer *flayer : fstack->layers) {
      Vector<ElementAnimations> animations = gather_animated_properties(mapping, *flayer);
      if (animations.is_empty()) {
        continue;
      }

      /* Create action for this layer. */
      std::string action_name = fstack->name.data;
      if (!STREQ(fstack->name.data, flayer->name.data) && fstack->layers.count != 1) {
        action_name += '|';
        action_name += flayer->name.data;
      }
      animrig::Action &action = animrig::action_add(bmain, action_name);
      id_fake_user_set(&action.id);
      action.layer_keystrip_ensure();
      animrig::StripKeyframeData &strip_data =
          action.layer(0)->strip(0)->data<animrig::StripKeyframeData>(action);

      /* Figure out the set of IDs that are animated. We want to preserve the order
       * of this set to match order of animations inside the FBX file. */
      VectorSet<ID *> animated_ids;
      Map<ID *, Vector<const ElementAnimations *>> id_to_anims;
      for (const ElementAnimations &anim : animations) {
        animated_ids.add(anim.target_id);
        Vector<const ElementAnimations *> &anims = id_to_anims.lookup_or_add_default(
            anim.target_id);
        anims.append(&anim);
      }

      /* Create action slots for each animated ID. */
      for (ID *id : animated_ids) {
        /* Create a slot for this ID. */
        BLI_assert(id != nullptr);
        const std::string slot_name = id->name;
        animrig::Slot &slot = action.slot_add_for_id_type(GS(id->name));
        action.slot_identifier_define(slot, slot_name);

        /* Assign this action & slot to ID. */
        const AnimData *adt = BKE_animdata_ensure_id(id);
        BLI_assert_msg(adt != nullptr, "fbx: could not create animation data for an ID");
        if (adt->action == nullptr) {
          bool ok = animrig::assign_action(&action, *id);
          BLI_assert_msg(ok, "fbx: could not assign action to ID");
          UNUSED_VARS_NDEBUG(ok);
        }
        if (adt->slot_handle == animrig::Slot::unassigned) {
          animrig::ActionSlotAssignmentResult res = animrig::assign_action_slot(&slot, *id);
          BLI_assert_msg(res == animrig::ActionSlotAssignmentResult::OK,
                         "fbx: failed to assign slot to ID");
          UNUSED_VARS_NDEBUG(res);
        }
        animrig::Channelbag &channelbag = strip_data.channelbag_for_slot_ensure(slot);

        /* Create animation curves for this ID. */
        Vector<const ElementAnimations *> id_anims = id_to_anims.lookup(id);
        /* Batch create the transform curves: creating them one by one is not very fast,
         * especially for armatures where many bones often are animated. So first create
         * their descriptors, then create the f-curves in one step, and finally fill their data. */
        Vector<animrig::FCurveDescriptor> curve_desc;
        Vector<int64_t> anim_transform_curve_index(id_anims.size());
        LinearAllocator name_alloc;
        for (const int64_t index : id_anims.index_range()) {
          const ElementAnimations *anim = id_anims[index];
          if (anim->prop_position || anim->prop_rotation || anim->prop_scale) {
            anim_transform_curve_index[index] = curve_desc.size();
            create_transform_curve_desc(mapping, *anim, name_alloc, curve_desc);
          }
          else {
            anim_transform_curve_index[index] = -1;
          }
        }
        blender::Vector<FCurve *> transform_curves;
        if (!curve_desc.is_empty()) {
          transform_curves = channelbag.fcurve_create_many(nullptr, curve_desc.as_span());
        }

        for (const int64_t index : id_anims.index_range()) {
          const ElementAnimations *anim = id_anims[index];
          if (anim->prop_position || anim->prop_rotation || anim->prop_scale) {
            create_transform_curve_data(mapping,
                                        flayer->anim,
                                        *anim,
                                        fps,
                                        anim_offset,
                                        transform_curves.data() +
                                            anim_transform_curve_index[index]);
          }
          if (anim->prop_focal_length || anim->prop_focus_dist) {
            create_camera_curves(fbx.metadata, *anim, channelbag, fps, anim_offset);
          }
          if (anim->prop_mat_diffuse) {
            create_material_curves(*anim, &action, channelbag, fps, anim_offset);
          }
          if (anim->prop_blend_shape) {
            create_blend_shape_curves(*anim, channelbag, fps, anim_offset);
          }
        }

        for (FCurve *curve : transform_curves) {
          finalize_curve(curve);
        }
      }
    }
  }
}

}  // namespace blender::io::fbx
