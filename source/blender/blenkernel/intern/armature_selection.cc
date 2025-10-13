/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "ANIM_armature.hh"

#include "BKE_armature.hh"

#include "BLI_listbase.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"

namespace blender::bke {

namespace {

void find_selected_bones__visit_bone(const bArmature *armature,
                                     SelectedBoneCallback callback,
                                     SelectedBonesResult &result,
                                     Bone *bone)
{
  const bool is_selected = blender::animrig::bone_is_selected(armature, bone);
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
  BKE_armature_find_selected_bones(armature, callback);
  return selected_bone_names;
}

BoneNameSet BKE_pose_channel_find_selected_names(const Object *object)
{
  if (!object->pose) {
    return {};
  }

  BoneNameSet selected_bone_names;
  LISTBASE_FOREACH (bPoseChannel *, pose_bone, &object->pose->chanbase) {
    if (pose_bone->flag & POSE_SELECTED) {
      selected_bone_names.add(pose_bone->name);
    }
  }
  return selected_bone_names;
}

}  // namespace blender::bke
