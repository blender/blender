/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "BLI_listbase.hh"
#include "BLI_math_rotation_c.hh"

#include "BKE_action.hh"
#include "BKE_animsys.hh"
#include "BKE_armature.hh"
#include "BKE_fcurve.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ANIM_action.hh"
#include "ANIM_pose.hh"
#include "ANIM_rna.hh"

namespace blender::animrig {

namespace {

using ActionApplier =
    FunctionRef<void(PointerRNA *, bAction *, slot_handle_t, const AnimationEvalContext *)>;

void pose_apply_restore_fcurves(const Span<FCurve *> fcurves)
{
  for (FCurve *fcu : fcurves) {
    fcu->flag &= ~FCURVE_DISABLED;
  }
}

/* Returns a vector of all FCurves on which the fcurve flag was modified. */
Vector<FCurve *> pose_apply_disable_fcurves_for_unselected_bones(
    bAction *action, const slot_handle_t slot_handle, const bke::BoneNameSet &selected_bone_names)
{
  Vector<FCurve *> modified_fcurves;
  auto disable_unselected_fcurve = [&](FCurve *fcu, const char *bone_name) {
    const bool is_bone_selected = selected_bone_names.contains(bone_name);
    if (!is_bone_selected) {
      if (!(fcu->flag & FCURVE_DISABLED)) {
        /* FCurve is not yet disabled, we need to reset that later. */
        modified_fcurves.append(fcu);
      }
      fcu->flag |= FCURVE_DISABLED;
    }
  };
  bke::BKE_action_find_fcurves_with_bones(action, slot_handle, disable_unselected_fcurve);
  return modified_fcurves;
}

void pose_apply(Object *ob,
                bAction *action,
                const slot_handle_t slot_handle,
                const AnimationEvalContext *anim_eval_context,
                ActionApplier applier)
{
  bPose *pose = ob->pose;
  if (pose == nullptr) {
    return;
  }

  if (action->wrap().slot_array_num == 0) {
    return;
  }

  const bke::BoneNameSet selected_bone_names = bke::BKE_pose_channel_find_selected_names(ob);

  /* Mute all FCurves that are not associated with selected bones. This separates the concept of
   * bone selection from the FCurve evaluation code. */
  Vector<FCurve *> modified_fcurves = pose_apply_disable_fcurves_for_unselected_bones(
      action, slot_handle, selected_bone_names);

  /* Apply the Action. */
  PointerRNA pose_owner_ptr = RNA_id_pointer_create(&ob->id);

  applier(&pose_owner_ptr, action, slot_handle, anim_eval_context);

  pose_apply_restore_fcurves(modified_fcurves);
}

}  // namespace

static bool is_fcurve_evaluatable(const FCurve &fcu)
{
  if (fcu.flag & (FCURVE_MUTED | FCURVE_DISABLED)) {
    return false;
  }
  if (fcu.grp != nullptr && (fcu.grp->flag & AGRP_MUTED)) {
    return false;
  }
  if (BKE_fcurve_is_empty(&fcu)) {
    return false;
  }
  return true;
}

/**
 * LERP between current value (blend_factor=0.0) and the value from the FCurve (blend_factor=1.0).
 */
static float get_fcurve_blend_value(FCurve &fcu,
                                    PathResolvedRNA &anim_rna,
                                    const AnimationEvalContext *anim_eval_context,
                                    const float blend_factor)
{
  const float fcurve_value = calculate_fcurve(&anim_rna, &fcu, anim_eval_context);

  float current_value;
  float value_to_write;
  if (!BKE_animsys_read_from_rna_path(&anim_rna, &current_value)) {
    /* Unable to read the current value for blending, so just apply the FCurve value instead. */
    return fcurve_value;
  }

  value_to_write = (1 - blend_factor) * current_value + blend_factor * fcurve_value;

  switch (RNA_property_type(anim_rna.prop)) {
    case PROP_BOOLEAN: /* Without this, anything less than 1.0 is converted to 'False' by
                        * ANIMSYS_FLOAT_AS_BOOL(). This is probably not desirable for blends,
                        * where anything above a 50% blend should act more like the FCurve than
                        * like the current value. */
    case PROP_INT:
    case PROP_ENUM:
      value_to_write = roundf(value_to_write);
      break;
      /* All other types are just handled as float, and value_to_write is already correct. */
    default:
      break;
  }
  return value_to_write;
}

static bool rotation_mode_is_euler(const eRotationModes rotation_mode)
{
  return rotation_mode >= ROT_MODE_EUL;
}

/**
 * This function assumes that the quaternion keys are sequential. They do not
 * have to be in array_index order. If the quaternion is only partially keyed,
 * the result is normalized. If it is fully keyed, the result is returned as-is.
 */
static void animsys_quaternion_evaluate_fcurves(PointerRNA &ptr,
                                                PropertyRNA *prop,
                                                const Span<FCurve *> quat_fcurves,
                                                const AnimationEvalContext *anim_eval_context,
                                                float r_quaternion[4])
{
  BLI_assert(quat_fcurves.size() <= 4);

  /* Initialize r_quaternion to the unit quaternion so that half-keyed quaternions at least have
   * *some* value in there. */
  r_quaternion[0] = 1.0f;
  r_quaternion[1] = 0.0f;
  r_quaternion[2] = 0.0f;
  r_quaternion[3] = 0.0f;
  PathResolvedRNA quat_rna;
  quat_rna.ptr = ptr;
  quat_rna.prop = prop;
  for (FCurve *quat_curve_fcu : quat_fcurves) {
    const int array_index = quat_curve_fcu->array_index;
    quat_rna.prop_index = array_index;
    r_quaternion[array_index] = calculate_fcurve(&quat_rna, quat_curve_fcu, anim_eval_context);
  }

  if (quat_fcurves.size() < 4) {
    /* This quaternion was incompletely keyed, so the result is a mixture of the unit quaternion
     * and values from FCurves. This means that it's almost certainly no longer of unit length. */
    normalize_qt(r_quaternion);
  }
}

/**
 * This function assumes that the quaternion keys are sequential. They do not
 * have to be in array_index order.
 */
static void animsys_blend_fcurves_quaternion(PointerRNA &ptr,
                                             PropertyRNA *prop,
                                             const Span<FCurve *> quaternion_fcurves,
                                             const AnimationEvalContext *anim_eval_context,
                                             const float blend_factor)
{
  BLI_assert(quaternion_fcurves.size() <= 4);

  float current_quat[4];
  RNA_property_float_get_array(&ptr, prop, current_quat);

  float target_quat[4];
  animsys_quaternion_evaluate_fcurves(
      ptr, prop, quaternion_fcurves, anim_eval_context, target_quat);

  float blended_quat[4];
  interp_qt_qtqt(blended_quat, current_quat, target_quat, blend_factor);

  RNA_property_float_set_array(&ptr, prop, blended_quat);
}

static void blend_rotation(PointerRNA &ptr,
                           PropertyRNA *prop,
                           const Span<FCurve *> rotation_fcurves,
                           const eRotationModes fcurve_rotation_mode,
                           const AnimationEvalContext *anim_eval_context,
                           const float blend_factor)
{
  if (fcurve_rotation_mode == ROT_MODE_QUAT) {
    animsys_blend_fcurves_quaternion(ptr, prop, rotation_fcurves, anim_eval_context, blend_factor);
    return;
  }

  PathResolvedRNA anim_rna;
  anim_rna.ptr = ptr;
  anim_rna.prop = prop;
  for (FCurve *fcurve : rotation_fcurves) {
    anim_rna.prop_index = fcurve->array_index;
    const float value_to_write = get_fcurve_blend_value(
        *fcurve, anim_rna, anim_eval_context, blend_factor);
    BKE_animsys_write_to_rna_path(&anim_rna, value_to_write);
  }
}

/**
 * Apply the rotation fcurves to the `ptr` by converting them to a matrix first. This means the
 * rotation can be applied regardless of rotation mode.
 *
 * \param blend_factor: LERP between the current rotation value of the `ptr` and the value of the
 * `rotation_fcurves`. A `1` means the `rotation_fcurves` will be applied at 100%.
 */
static void blend_rotation_with_conversion(PointerRNA &ptr,
                                           const Span<FCurve *> rotation_fcurves,
                                           const eRotationModes fcurve_rotation_mode,
                                           const float eval_time,
                                           const float blend_factor)
{
  /* The rotation data is 0 initialized for reasonable defaults in case some indices have no
   * FCurves associated with them. */
  float4 fcurve_rotation_values(0.0);
  if (fcurve_rotation_mode == ROT_MODE_QUAT) {
    /* Default W value for quaternions. */
    fcurve_rotation_values[0] = 1.0;
  }

  for (FCurve *fcurve : rotation_fcurves) {
    BLI_assert_msg(fcurve->array_index >= 0 && fcurve->array_index < 4,
                   "Rotation properties have at most 4 components.");
    fcurve_rotation_values[fcurve->array_index] = evaluate_fcurve(fcurve, eval_time);
  }

  /* Converting to quaternion simplifies blending below. */
  float4 fcurve_quat;
  switch (fcurve_rotation_mode) {
    case ROT_MODE_QUAT: {
      copy_qt_qt(fcurve_quat, fcurve_rotation_values);
      break;
    }
    case ROT_MODE_EUL: {
      /* TODO: determine the rotation order for euler angles. This has to be stored at the
       * point of pose creation. */
      eulO_to_quat(fcurve_quat, fcurve_rotation_values, ROT_MODE_XYZ);
      break;
    }
    case ROT_MODE_AXISANGLE: {
      axis_angle_to_quat(fcurve_quat, &fcurve_rotation_values[1], fcurve_rotation_values[0]);
      break;
    }
    default: {
      BLI_assert_unreachable();
    }
  }

  float4 interp_quat;
  if (ptr.type == RNA_PoseBone) {
    bPoseChannel *pose_bone = static_cast<bPoseChannel *>(ptr.data);
    const float4 quat = BKE_pchan_rot_to_quat(*pose_bone);
    interp_qt_qtqt(interp_quat, quat, fcurve_quat, blend_factor);
    BKE_pchan_quat_to_rot(*pose_bone, interp_quat);
  }
  else if (ptr.type == RNA_Object) {
    Object *object = static_cast<Object *>(ptr.data);
    const float4 quat = BKE_object_rot_to_quat(*object);
    interp_qt_qtqt(interp_quat, quat, fcurve_quat, blend_factor);
    BKE_object_quat_to_rot(*object, interp_quat);
  }
  else {
    BLI_assert_unreachable();
  }
}

/* LERP between current value (blend_factor=0.0) and the value from the FCurve (blend_factor=1.0)
 */
static void animsys_blend_in_fcurves(PointerRNA &ptr,
                                     const Span<FCurve *> fcurves,
                                     const AnimationEvalContext *anim_eval_context,
                                     const float blend_factor)
{
  /* Rotations are a special case since the rotation mode of the pose may not match with the
   * current rotation mode of the `ptr`. Also quaternions need to be handled together. */
  Map<StringRefNull, Vector<FCurve *>> rotation_fcurve_map;
  for (FCurve *fcurve : fcurves) {
    StringRefNull rna_path = fcurve->rna_path();

    if (!is_fcurve_evaluatable(*fcurve)) {
      continue;
    }

    if (!is_rotation_path(rna_path)) {
      continue;
    }

    Vector<FCurve *> &rotation_fcurves = rotation_fcurve_map.lookup_or_add_default(rna_path);
    rotation_fcurves.append(fcurve);
  }

  for (const auto &[rna_path, rotation_fcurves] : rotation_fcurve_map.items()) {
    PointerRNA resolved_ptr;
    PropertyRNA *resolved_prop;
    if (!RNA_path_resolve_property(&ptr, rna_path.data(), &resolved_ptr, &resolved_prop)) {
      continue;
    }

    std::optional<eRotationModes> ptr_rotation_mode_opt = get_rotation_mode_from_rna_pointer(
        resolved_ptr);
    BLI_assert_msg(
        ptr_rotation_mode_opt.has_value(),
        "We have an FCurve on a rotation property, the RNA data should have a rotation order.");
    const eRotationModes ptr_rotation_mode = ptr_rotation_mode_opt.value();

    const std::optional<eRotationModes> fcurve_rotation_mode_opt = get_rotation_mode_from_path(
        rna_path);
    BLI_assert(fcurve_rotation_mode_opt.has_value());
    const eRotationModes fcurve_rotation_mode = fcurve_rotation_mode_opt.value();

    /* The check for Euler rotation mode means we will *not* do any conversion if both modes are
     * euler. Since we *cannot* know the exact euler mode of the stored FCurves we have to assume
     * they are the same as the ptr. */
    if (fcurve_rotation_mode == ptr_rotation_mode ||
        (rotation_mode_is_euler(fcurve_rotation_mode) &&
         rotation_mode_is_euler(ptr_rotation_mode)))
    {
      /* Easy case, animation mode of fcurves and of `resolved_ptr` are matching. Data can just
       * be applied. The reason to have this separate is because in this case euler angles > 180
       * degrees are preserved. The other path uses a conversion to a quaternion which loses that
       * information. */
      blend_rotation(resolved_ptr,
                     resolved_prop,
                     rotation_fcurves,
                     fcurve_rotation_mode,
                     anim_eval_context,
                     blend_factor);
    }
    else {
      blend_rotation_with_conversion(resolved_ptr,
                                     rotation_fcurves,
                                     fcurve_rotation_mode,
                                     anim_eval_context->eval_time,
                                     blend_factor);
    }
  }

  for (FCurve *fcu : fcurves) {
    if (!is_fcurve_evaluatable(*fcu)) {
      continue;
    }

    const StringRefNull rna_path = fcu->rna_path();
    if (rotation_fcurve_map.contains(rna_path)) {
      continue;
    }

    PathResolvedRNA anim_rna;
    if (!BKE_animsys_rna_path_resolve(&ptr, rna_path.c_str(), fcu->array_index, &anim_rna)) {
      continue;
    }

    const float value_to_write = get_fcurve_blend_value(
        *fcu, anim_rna, anim_eval_context, blend_factor);
    BKE_animsys_write_to_rna_path(&anim_rna, value_to_write);
  }
}

static void blend_in_action(PointerRNA &ptr,
                            Action &act,
                            const int32_t action_slot_handle,
                            const AnimationEvalContext *anim_eval_context,
                            const float blend_factor)
{
  Vector<FCurve *> fcurves = fcurves_for_action_slot(act, action_slot_handle);
  animsys_blend_in_fcurves(ptr, fcurves, anim_eval_context, blend_factor);
}

void pose_apply_action_all_bones(Object *ob,
                                 bAction *action,
                                 const int32_t slot_handle,
                                 const AnimationEvalContext *anim_eval_context)
{
  PointerRNA pose_owner_ptr = RNA_id_pointer_create(&ob->id);
  animsys_evaluate_action(&pose_owner_ptr, action, slot_handle, anim_eval_context, false);
}

void pose_apply_action_blend(Object *ob,
                             bAction *action,
                             const int32_t slot_handle,
                             const AnimationEvalContext *anim_eval_context,
                             const float blend_factor)
{
  auto evaluate_and_blend = [blend_factor](PointerRNA *ptr,
                                           bAction *act,
                                           const int32_t slot_handle,
                                           const AnimationEvalContext *anim_eval_context) {
    blend_in_action(*ptr, act->wrap(), slot_handle, anim_eval_context, blend_factor);
  };

  pose_apply(ob, action, slot_handle, anim_eval_context, evaluate_and_blend);
}

void pose_apply_action_blend_all_bones(Object *ob,
                                       bAction *action,
                                       slot_handle_t slot_handle,
                                       const AnimationEvalContext *anim_eval_context,
                                       const float blend_factor)
{
  PointerRNA pose_owner_ptr = RNA_id_pointer_create(&ob->id);
  blend_in_action(pose_owner_ptr, action->wrap(), slot_handle, anim_eval_context, blend_factor);
}

bool any_bone_selected(const Span<const Object *> objects)
{
  for (const Object *obj : objects) {
    if (!obj->pose) {
      continue;
    }
    for (bPoseChannel &pose_bone : obj->pose->chanbase) {
      if (pose_bone.flag & POSE_SELECTED) {
        return true;
      }
    }
  }
  return false;
}

void pose_apply_action(const Span<Object *> objects,
                       Action &pose_action,
                       const AnimationEvalContext *anim_eval_context,
                       const float blend_factor)
{
  if (any_bone_selected(objects)) {
    for (Object *object : objects) {
      Slot &slot = get_best_pose_slot_for_id(object->id, pose_action);
      pose_apply_action_blend(object, &pose_action, slot.handle, anim_eval_context, blend_factor);
    }
  }
  else {
    /* In the case of nothing selected, act as if all is selected. This is a convenience feature
     * for the artists so they don't have to be specific in their selection all the time. */
    for (Object *object : objects) {
      Slot &slot = get_best_pose_slot_for_id(object->id, pose_action);
      pose_apply_action_blend_all_bones(
          object, &pose_action, slot.handle, anim_eval_context, blend_factor);
    }
  }
}

Slot &get_best_pose_slot_for_id(const ID &id, Action &pose_data)
{
  BLI_assert_msg(pose_data.slot_array_num > 0,
                 "Actions without slots have no data. This should have been caught earlier.");

  Slot *slot = generic_slot_for_autoassign(id, pose_data, "");
  if (slot == nullptr) {
    slot = pose_data.slot(0);
  }

  return *slot;
}

}  // namespace blender::animrig
