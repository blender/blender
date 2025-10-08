/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_pose_backup.h"

#include <cstring>

#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_idprop.hh"
#include "BKE_object_types.hh"

#include "ANIM_action.hh"
#include "ANIM_pose.hh"

using namespace blender::bke;

/* simple struct for storing backup info for one pose channel */
struct PoseChannelBackup {
  PoseChannelBackup *next, *prev;

  bPoseChannel *pchan; /* Pose channel this backup is for. */

  bPoseChannel olddata; /* Backup of pose channel. */
  IDProperty *oldprops; /* Backup copy (needs freeing) of pose channel's ID properties. */
  /* Backup copy (needs freeing) of pose channel's system IDProperties. */
  IDProperty *old_system_properties;
  const Object *owner; /* The object to which this pose channel belongs. */
};

struct PoseBackup {
  bool is_bone_selection_relevant;
  ListBase /*PoseChannelBackup*/ backups;
};

/**
 * Create a backup of the pose, for only those bones that are animated in the
 * given Action. If `selected_bone_names` is not empty, the set of bones to back
 * up is intersected with these bone names such that only the selected subset is
 * backed up.
 *
 * The returned pointer is owned by the caller.
 */
static void pose_backup_create(const Object *ob,
                               bAction *action,
                               const BoneNameSet &selected_bone_names,
                               PoseBackup &pose_backup)
{
  BoneNameSet backed_up_bone_names;
  const bool is_bone_selection_relevant = pose_backup.is_bone_selection_relevant;
  /* Make a backup of the given pose channel. */
  auto store_animated_pchans = [&](const FCurve * /*unused*/, const char *bone_name) {
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

    PoseChannelBackup *chan_bak = MEM_callocN<PoseChannelBackup>("PoseChannelBackup");
    chan_bak->pchan = pchan;
    chan_bak->olddata = blender::dna::shallow_copy(*chan_bak->pchan);
    chan_bak->owner = ob;

    if (pchan->prop) {
      chan_bak->oldprops = IDP_CopyProperty(pchan->prop);
    }
    if (pchan->system_properties) {
      chan_bak->old_system_properties = IDP_CopyProperty(pchan->system_properties);
    }

    BLI_addtail(&pose_backup.backups, chan_bak);
    backed_up_bone_names.add_new(bone_name);
  };

  blender::animrig::Slot &slot = blender::animrig::get_best_pose_slot_for_id(ob->id,
                                                                             action->wrap());
  /* Call `store_animated_pchans()` for each FCurve that targets a bone. */
  BKE_action_find_fcurves_with_bones(action, slot.handle, store_animated_pchans);
}

static blender::Set<bPoseChannel *> armature_find_selected_pose_bones(
    blender::Span<Object *> objects)
{
  blender::Set<bPoseChannel *> selected_bones;
  bool all_bones_selected = true;

  for (Object *obj : objects) {
    /* Iterate over the selected bones to fill the set of bone names. */
    LISTBASE_FOREACH (bPoseChannel *, pose_bone, &obj->pose->chanbase) {
      if (pose_bone->flag & POSE_SELECTED) {
        selected_bones.add(pose_bone);
      }
      else {
        all_bones_selected = false;
      }
    }
  }

  /* If all bones are selected, act as if none are. */
  if (all_bones_selected) {
    return {};
  }

  return selected_bones;
}

PoseBackup *BKE_pose_backup_create_all_bones(blender::Span<Object *> objects,
                                             const bAction *action)
{
  PoseBackup *pose_backup = MEM_callocN<PoseBackup>(__func__);
  pose_backup->backups = {nullptr, nullptr};
  pose_backup->is_bone_selection_relevant = false;
  for (Object *ob : objects) {
    pose_backup_create(ob, const_cast<bAction *>(action), BoneNameSet(), *pose_backup);
  }
  return pose_backup;
}

PoseBackup *BKE_pose_backup_create_selected_bones(blender::Span<Object *> objects,
                                                  const bAction *action)
{
  PoseBackup *pose_backup = MEM_callocN<PoseBackup>(__func__);
  pose_backup->backups = {nullptr, nullptr};
  blender::Set<bPoseChannel *> selected_bones = armature_find_selected_pose_bones(objects);
  pose_backup->is_bone_selection_relevant = !selected_bones.is_empty();

  for (Object *ob : objects) {
    const BoneNameSet selected_bone_names = BKE_pose_channel_find_selected_names(ob);
    pose_backup_create(ob, const_cast<bAction *>(action), selected_bone_names, *pose_backup);
  }

  return pose_backup;
}

bool BKE_pose_backup_is_selection_relevant(const PoseBackup *pose_backup)
{
  return pose_backup->is_bone_selection_relevant;
}

void BKE_pose_backup_restore(const PoseBackup *pbd)
{
  LISTBASE_FOREACH (PoseChannelBackup *, chan_bak, &pbd->backups) {
    *chan_bak->pchan = blender::dna::shallow_copy(chan_bak->olddata);

    if (chan_bak->oldprops) {
      IDP_SyncGroupValues(chan_bak->pchan->prop, chan_bak->oldprops);
    }
    if (chan_bak->old_system_properties) {
      IDP_SyncGroupValues(chan_bak->pchan->system_properties, chan_bak->old_system_properties);
    }

    /* TODO: constraints settings aren't restored yet,
     * even though these could change (though not that likely) */
  }
}

void BKE_pose_backup_free(PoseBackup *pbd)
{
  if (!pbd) {
    /* Can happen if initialization was aborted. */
    return;
  }
  LISTBASE_FOREACH_MUTABLE (PoseChannelBackup *, chan_bak, &pbd->backups) {
    if (chan_bak->oldprops) {
      IDP_FreeProperty(chan_bak->oldprops);
    }
    if (chan_bak->old_system_properties) {
      IDP_FreeProperty(chan_bak->old_system_properties);
    }
    BLI_freelinkN(&pbd->backups, chan_bak);
  }
  MEM_freeN(pbd);
}

void BKE_pose_backup_create_on_object(Object *ob, const bAction *action)
{
  BKE_pose_backup_clear(ob);
  PoseBackup *pose_backup = BKE_pose_backup_create_all_bones({ob}, action);
  ob->runtime->pose_backup = pose_backup;
}

bool BKE_pose_backup_restore_on_object(Object *ob)
{
  if (ob->runtime->pose_backup == nullptr) {
    return false;
  }
  BKE_pose_backup_restore(ob->runtime->pose_backup);
  return true;
}

void BKE_pose_backup_clear(Object *ob)
{
  if (ob->runtime->pose_backup == nullptr) {
    return;
  }

  BKE_pose_backup_free(ob->runtime->pose_backup);
  ob->runtime->pose_backup = nullptr;
}
