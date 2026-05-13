/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_armature.hh"

#include "BKE_action.hh"
#include "BKE_pose.hh"

#include "BLI_listbase.h"

#include "DNA_object_types.h"

namespace blender::animrig {

void pose_bone_descendent_iterator(Object &pose_ob,
                                   bPoseChannel &pchan,
                                   FunctionRef<void(bPoseChannel &child_bone)> callback)
{
  /* Needed for fast name lookups. */
  BKE_pose_channels_hash_ensure(pose_ob.pose);

  int i = 0;
  /* This is not using an std::deque because the implementation of that has issues on windows. */
  Vector<bPoseChannel *> descendants = {&pchan};
  while (i < descendants.size()) {
    bPoseChannel *descendant = descendants[i];
    i++;
    callback(*descendant);
    Bone *descendant_bone = descendant->bone_get(pose_ob);
    for (Bone &child_bone : descendant_bone->childbase) {
      bPoseChannel *child_pose_bone = BKE_pose_channel_find_name(pose_ob.pose, child_bone.name);
      if (!child_pose_bone) {
        /* Can happen if the pose is not rebuilt. */
        BLI_assert_unreachable();
        continue;
      }
      descendants.append(child_pose_bone);
    }
  }
};

static bool pose_depth_iterator_recursive(Object &pose_ob,
                                          bke::PChanBone pchanbone,
                                          FunctionRef<bool(bPoseChannel &child_bone)> callback)
{
  if (!callback(*pchanbone.pchan)) {
    return false;
  }
  bool success = true;
  for (Bone &child_bone : pchanbone.bone->childbase) {
    bPoseChannel *child_pose_bone = BKE_pose_channel_find_name(pose_ob.pose, child_bone.name);
    if (!child_pose_bone) {
      BLI_assert_unreachable();
      success = false;
      continue;
    }
    success &= pose_depth_iterator_recursive(pose_ob, {child_pose_bone, &child_bone}, callback);
  }
  return success;
}

bool pose_bone_descendent_depth_iterator(Object &pose_ob,
                                         bPoseChannel &pchan,
                                         FunctionRef<bool(bPoseChannel &child_bone)> callback)
{
  /* Needed for fast name lookups. */
  BKE_pose_channels_hash_ensure(pose_ob.pose);
  return pose_depth_iterator_recursive(pose_ob, {&pchan, pchan.bone_get(pose_ob)}, callback);
}

}  // namespace blender::animrig
