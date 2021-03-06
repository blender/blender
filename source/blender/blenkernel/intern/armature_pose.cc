/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Defines and code for core node types
 */

/** \file
 * \ingroup bke
 */

#include "BKE_animsys.h"
#include "BKE_armature.h"

#include "BLI_set.hh"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"

namespace {
using BoneNameSet = blender::Set<std::string>;

// Forward declarations.
BoneNameSet pose_apply_find_selected_bones(const bPose *pose);
void pose_apply_disable_fcurves_for_unselected_bones(bAction *action,
                                                     const BoneNameSet &selected_bone_names);
void pose_apply_restore_fcurves(bAction *action);
}  // namespace

void BKE_pose_apply_action(struct Object *ob,
                           struct bAction *action,
                           struct AnimationEvalContext *anim_eval_context)
{
  bPose *pose = ob->pose;
  if (pose == nullptr) {
    return;
  }

  const BoneNameSet selected_bone_names = pose_apply_find_selected_bones(pose);
  const bool limit_to_selected_bones = !selected_bone_names.is_empty();

  if (limit_to_selected_bones) {
    /* Mute all FCurves that are not associated with selected bones. This separates the concept of
     * bone selection from the FCurve evaluation code. */
    pose_apply_disable_fcurves_for_unselected_bones(action, selected_bone_names);
  }

  /* Apply the Action. */
  PointerRNA pose_owner_ptr;
  RNA_id_pointer_create(&ob->id, &pose_owner_ptr);
  animsys_evaluate_action(&pose_owner_ptr, action, anim_eval_context, false);

  if (limit_to_selected_bones) {
    pose_apply_restore_fcurves(action);
  }
}

namespace {
BoneNameSet pose_apply_find_selected_bones(const bPose *pose)
{
  BoneNameSet selected_bone_names;
  bool all_bones_selected = true;
  bool no_bones_selected = true;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    const bool is_selected = (pchan->bone->flag & BONE_SELECTED) != 0 &&
                             (pchan->bone->flag & BONE_HIDDEN_P) == 0;
    all_bones_selected &= is_selected;
    no_bones_selected &= !is_selected;

    if (is_selected) {
      /* Bone names are unique, so no need to check for duplicates. */
      selected_bone_names.add_new(pchan->name);
    }
  }

  /* If no bones are selected, act as if all are. */
  if (all_bones_selected || no_bones_selected) {
    return BoneNameSet(); /* An empty set means "ignore bone selection". */
  }
  return selected_bone_names;
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
  LISTBASE_FOREACH (FCurve *, fcu, &action->curves) {
    if (!fcu->rna_path || !strstr(fcu->rna_path, "pose.bones[")) {
      continue;
    }

    /* Get bone name, and check if this bone is selected. */
    char *bone_name = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
    if (!bone_name) {
      continue;
    }
    const bool is_selected = selected_bone_names.contains(bone_name);
    MEM_freeN(bone_name);
    if (is_selected) {
      continue;
    }

    fcu->flag |= FCURVE_DISABLED;
  }
}

}  // namespace
