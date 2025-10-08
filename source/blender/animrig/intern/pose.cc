/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_pose.hh"
#include "BKE_action.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BLI_listbase.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "RNA_access.hh"

#include "ANIM_action.hh"

namespace blender::animrig {

namespace {

using ActionApplier = blender::FunctionRef<void(
    PointerRNA *, bAction *, slot_handle_t, const AnimationEvalContext *)>;

void pose_apply_restore_fcurves(const Span<FCurve *> fcurves)
{
  for (FCurve *fcu : fcurves) {
    fcu->flag &= ~FCURVE_DISABLED;
  }
}

/* Returns a vector of all FCurves on which the fcurve flag was modified. */
Vector<FCurve *> pose_apply_disable_fcurves_for_unselected_bones(
    bAction *action,
    const slot_handle_t slot_handle,
    const blender::bke::BoneNameSet &selected_bone_names)
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
  blender::bke::BKE_action_find_fcurves_with_bones(action, slot_handle, disable_unselected_fcurve);
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

  const blender::bke::BoneNameSet selected_bone_names =
      blender::bke::BKE_pose_channel_find_selected_names(ob);

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
    animsys_blend_in_action(ptr, act, slot_handle, anim_eval_context, blend_factor);
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
  animsys_blend_in_action(&pose_owner_ptr, action, slot_handle, anim_eval_context, blend_factor);
}

bool any_bone_selected(const blender::Span<const Object *> objects)
{
  for (const Object *obj : objects) {
    if (!obj->pose) {
      continue;
    }
    LISTBASE_FOREACH (bPoseChannel *, pose_bone, &obj->pose->chanbase) {
      if (pose_bone->flag & POSE_SELECTED) {
        return true;
      }
    }
  }
  return false;
}

void pose_apply_action(const blender::Span<Object *> objects,
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
