/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_armature.hh"

#include "BLI_listbase.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"

namespace blender::bke {

namespace {

void find_selected_bones__visit_bone(const bArmature *armature,
                                     SelectedBoneCallback callback,
                                     SelectedBonesResult &result,
                                     Bone *bone)
{
  const bool is_selected = PBONE_SELECTED(armature, bone);
  result.all_bones_selected &= is_selected;
  result.no_bones_selected &= !is_selected;

  if (is_selected) {
    callback(bone);
  }

  LISTBASE_FOREACH (Bone *, child_bone, &bone->childbase) {
    find_selected_bones__visit_bone(armature, callback, result, child_bone);
  }
}
}  // namespace

SelectedBonesResult BKE_armature_find_selected_bones(const bArmature *armature,
                                                     SelectedBoneCallback callback)
{
  SelectedBonesResult result;
  LISTBASE_FOREACH (Bone *, root_bone, &armature->bonebase) {
    find_selected_bones__visit_bone(armature, callback, result, root_bone);
  }

  return result;
}

BoneNameSet BKE_armature_find_selected_bone_names(const bArmature *armature)
{
  BoneNameSet selected_bone_names;

  /* Iterate over the selected bones to fill the set of bone names. */
  auto callback = [&](Bone *bone) { selected_bone_names.add(bone->name); };
  SelectedBonesResult result = BKE_armature_find_selected_bones(armature, callback);

  /* If no bones are selected, act as if all are. */
  if (result.all_bones_selected || result.no_bones_selected) {
    return BoneNameSet();
  }

  return selected_bone_names;
}

}  // namespace blender::bke
