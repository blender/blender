/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_action.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"

#include "BLI_function_ref.hh"
#include "BLI_set.hh"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"

using namespace blender::bke;

namespace {

using ActionApplier =
    blender::FunctionRef<void(PointerRNA *, bAction *, const AnimationEvalContext *)>;

/* Forward declarations. */
void pose_apply_disable_fcurves_for_unselected_bones(bAction *action,
                                                     const BoneNameSet &selected_bone_names);
void pose_apply_restore_fcurves(bAction *action);

void pose_apply(struct Object *ob,
                struct bAction *action,
                struct AnimationEvalContext *anim_eval_context,
                ActionApplier applier);

}  // namespace

void BKE_pose_apply_action_selected_bones(struct Object *ob,
                                          struct bAction *action,
                                          struct AnimationEvalContext *anim_eval_context)
{
  auto evaluate_and_apply =
      [](PointerRNA *ptr, bAction *act, const AnimationEvalContext *anim_eval_context) {
        animsys_evaluate_action(ptr, act, anim_eval_context, false);
      };

  pose_apply(ob, action, anim_eval_context, evaluate_and_apply);
}

void BKE_pose_apply_action_all_bones(struct Object *ob,
                                     struct bAction *action,
                                     struct AnimationEvalContext *anim_eval_context)
{
  PointerRNA pose_owner_ptr;
  RNA_id_pointer_create(&ob->id, &pose_owner_ptr);
  animsys_evaluate_action(&pose_owner_ptr, action, anim_eval_context, false);
}

void BKE_pose_apply_action_blend(struct Object *ob,
                                 struct bAction *action,
                                 struct AnimationEvalContext *anim_eval_context,
                                 const float blend_factor)
{
  auto evaluate_and_blend = [blend_factor](PointerRNA *ptr,
                                           bAction *act,
                                           const AnimationEvalContext *anim_eval_context) {
    animsys_blend_in_action(ptr, act, anim_eval_context, blend_factor);
  };

  pose_apply(ob, action, anim_eval_context, evaluate_and_blend);
}

namespace {
void pose_apply(struct Object *ob,
                struct bAction *action,
                struct AnimationEvalContext *anim_eval_context,
                ActionApplier applier)
{
  bPose *pose = ob->pose;
  if (pose == nullptr) {
    return;
  }

  const bArmature *armature = (bArmature *)ob->data;
  const BoneNameSet selected_bone_names = BKE_armature_find_selected_bone_names(armature);
  const bool limit_to_selected_bones = !selected_bone_names.is_empty();

  if (limit_to_selected_bones) {
    /* Mute all FCurves that are not associated with selected bones. This separates the concept of
     * bone selection from the FCurve evaluation code. */
    pose_apply_disable_fcurves_for_unselected_bones(action, selected_bone_names);
  }

  /* Apply the Action. */
  PointerRNA pose_owner_ptr;
  RNA_id_pointer_create(&ob->id, &pose_owner_ptr);

  applier(&pose_owner_ptr, action, anim_eval_context);

  if (limit_to_selected_bones) {
    pose_apply_restore_fcurves(action);
  }
}

void pose_apply_restore_fcurves(bAction *action)
{
  /* TODO(Sybren): Restore the FCurve flags, instead of just erasing the 'disabled' flag. */
  LISTBASE_FOREACH (FCurve *, fcu, &action->curves) {
    fcu->flag &= ~FCURVE_DISABLED;
  }
}

void pose_apply_disable_fcurves_for_unselected_bones(bAction *action,
                                                     const BoneNameSet &selected_bone_names)
{
  auto disable_unselected_fcurve = [&](FCurve *fcu, const char *bone_name) {
    const bool is_bone_selected = selected_bone_names.contains(bone_name);
    if (!is_bone_selected) {
      fcu->flag |= FCURVE_DISABLED;
    }
  };
  BKE_action_find_fcurves_with_bones(action, disable_unselected_fcurve);
}

}  // namespace
