/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include "ED_armature.h"

#include <cstring>

#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_idprop.h"

using namespace blender::bke;

/* simple struct for storing backup info for one pose channel */
struct PoseChannelBackup {
  struct PoseChannelBackup *next, *prev;

  struct bPoseChannel *pchan; /* Pose channel this backup is for. */

  struct bPoseChannel olddata; /* Backup of pose channel. */
  struct IDProperty *oldprops; /* Backup copy (needs freeing) of pose channel's ID properties. */
};

struct PoseBackup {
  bool is_bone_selection_relevant;
  ListBase /* PoseChannelBackup* */ backups;
};

static PoseBackup *pose_backup_create(const Object *ob,
                                      const bAction *action,
                                      const BoneNameSet &selected_bone_names)
{
  ListBase backups = {nullptr, nullptr};
  const bool is_bone_selection_relevant = !selected_bone_names.is_empty();

  BoneNameSet backed_up_bone_names;
  /* Make a backup of the given pose channel. */
  auto store_animated_pchans = [&](FCurve * /* unused */, const char *bone_name) {
    if (backed_up_bone_names.contains(bone_name)) {
      /* Only backup each bone once. */
      return;
    }

    bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
    if (pchan == nullptr) {
      /* FCurve targets non-existent bone. */
      return;
    }

    if (is_bone_selection_relevant && !selected_bone_names.contains(bone_name)) {
      return;
    }

    PoseChannelBackup *chan_bak = static_cast<PoseChannelBackup *>(
        MEM_callocN(sizeof(*chan_bak), "PoseChannelBackup"));
    chan_bak->pchan = pchan;
    memcpy(&chan_bak->olddata, chan_bak->pchan, sizeof(chan_bak->olddata));

    if (pchan->prop) {
      chan_bak->oldprops = IDP_CopyProperty(pchan->prop);
    }

    BLI_addtail(&backups, chan_bak);
    backed_up_bone_names.add_new(bone_name);
  };

  /* Call `store_animated_pchans()` for each FCurve that targets a bone. */
  BKE_action_find_fcurves_with_bones(action, store_animated_pchans);

  /* PoseBackup is constructed late, so that the above loop can use stack variables. */
  PoseBackup *pose_backup = static_cast<PoseBackup *>(MEM_callocN(sizeof(*pose_backup), __func__));
  pose_backup->is_bone_selection_relevant = is_bone_selection_relevant;
  pose_backup->backups = backups;
  return pose_backup;
}

PoseBackup *ED_pose_backup_create_all_bones(const Object *ob, const bAction *action)
{
  return pose_backup_create(ob, action, BoneNameSet());
}

PoseBackup *ED_pose_backup_create_selected_bones(const Object *ob, const bAction *action)
{
  const bArmature *armature = static_cast<const bArmature *>(ob->data);
  const BoneNameSet selected_bone_names = BKE_armature_find_selected_bone_names(armature);
  return pose_backup_create(ob, action, selected_bone_names);
}

bool ED_pose_backup_is_selection_relevant(const struct PoseBackup *pose_backup)
{
  return pose_backup->is_bone_selection_relevant;
}

void ED_pose_backup_restore(const PoseBackup *pbd)
{
  LISTBASE_FOREACH (PoseChannelBackup *, chan_bak, &pbd->backups) {
    memcpy(chan_bak->pchan, &chan_bak->olddata, sizeof(chan_bak->olddata));

    if (chan_bak->oldprops) {
      IDP_SyncGroupValues(chan_bak->pchan->prop, chan_bak->oldprops);
    }

    /* TODO: constraints settings aren't restored yet,
     * even though these could change (though not that likely) */
  }
}

void ED_pose_backup_free(PoseBackup *pbd)
{
  LISTBASE_FOREACH_MUTABLE (PoseChannelBackup *, chan_bak, &pbd->backups) {
    if (chan_bak->oldprops) {
      IDP_FreeProperty(chan_bak->oldprops);
    }
    BLI_freelinkN(&pbd->backups, chan_bak);
  }
  MEM_freeN(pbd);
}
