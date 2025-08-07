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

  std::deque<bPoseChannel *> to_visit = {&pose_bone};
  while (!to_visit.empty()) {
    bPoseChannel *descendant = to_visit.front();
    to_visit.pop_front();
    callback(*descendant);
    LISTBASE_FOREACH (Bone *, child_bone, &descendant->bone->childbase) {
      bPoseChannel *child_pose_bone = BKE_pose_channel_find_name(&pose, child_bone->name);
      if (!child_pose_bone) {
        /* Can happen if the pose is not rebuilt. */
        BLI_assert_unreachable();
        continue;
      }
      to_visit.push_back(child_pose_bone);
    }
  }
};

}  // namespace blender::animrig
