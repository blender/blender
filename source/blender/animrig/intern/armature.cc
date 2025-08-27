/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include <deque>

#include "ANIM_armature.hh"
#include "BKE_action.hh"
#include "BLI_listbase.h"

namespace blender::animrig {

void pose_bone_descendent_iterator(bPose &pose,
                                   bPoseChannel &pose_bone,
                                   FunctionRef<void(bPoseChannel &child_bone)> callback)
{
  /* Needed for fast name lookups. */
  BKE_pose_channels_hash_ensure(&pose);

  int i = 0;
  /* This is not using an std::deque because the implementation of that has issues on windows. */
  Vector<bPoseChannel *> descendants = {&pose_bone};
  while (i < descendants.size()) {
    bPoseChannel *descendant = descendants[i];
    i++;
    callback(*descendant);
    LISTBASE_FOREACH (Bone *, child_bone, &descendant->bone->childbase) {
      bPoseChannel *child_pose_bone = BKE_pose_channel_find_name(&pose, child_bone->name);
      if (!child_pose_bone) {
        /* Can happen if the pose is not rebuilt. */
        BLI_assert_unreachable();
        continue;
      }
      descendants.append(child_pose_bone);
    }
  }
};

}  // namespace blender::animrig
