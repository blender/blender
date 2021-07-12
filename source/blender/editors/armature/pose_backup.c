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
 */

/** \file
 * \ingroup edarmature
 */

#include "ED_armature.h"

#include <string.h>

#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_idprop.h"

/* simple struct for storing backup info for one pose channel */
typedef struct PoseChannelBackup {
  struct PoseChannelBackup *next, *prev;

  struct bPoseChannel *pchan; /* Pose channel this backup is for. */

  struct bPoseChannel olddata; /* Backup of pose channel. */
  struct IDProperty *oldprops; /* Backup copy (needs freeing) of pose channel's ID properties. */
} PoseChannelBackup;

typedef struct PoseBackup {
  bool is_bone_selection_relevant;
  ListBase /* PoseChannelBackup* */ backups;
} PoseBackup;

static PoseBackup *pose_backup_create(const Object *ob,
                                      const bAction *action,
                                      const bool is_bone_selection_relevant)
{
  ListBase backups = {NULL, NULL};
  const bArmature *armature = ob->data;

  /* TODO(Sybren): reuse same approach as in `armature_pose.cc` in this function, as that doesn't
   * have the assumption that action group names are bone names. */
  LISTBASE_FOREACH (bActionGroup *, agrp, &action->groups) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, agrp->name);
    if (pchan == NULL) {
      continue;
    }

    if (is_bone_selection_relevant && !PBONE_SELECTED(armature, pchan->bone)) {
      continue;
    }

    PoseChannelBackup *chan_bak = MEM_callocN(sizeof(*chan_bak), "PoseChannelBackup");
    chan_bak->pchan = pchan;
    memcpy(&chan_bak->olddata, chan_bak->pchan, sizeof(chan_bak->olddata));

    if (pchan->prop) {
      chan_bak->oldprops = IDP_CopyProperty(pchan->prop);
    }

    BLI_addtail(&backups, chan_bak);
  }

  /* PoseBackup is constructed late, so that the above loop can use stack variables. */
  PoseBackup *pose_backup = MEM_callocN(sizeof(*pose_backup), __func__);
  pose_backup->is_bone_selection_relevant = is_bone_selection_relevant;
  pose_backup->backups = backups;
  return pose_backup;
}

PoseBackup *ED_pose_backup_create_all_bones(const Object *ob, const bAction *action)
{
  return pose_backup_create(ob, action, false);
}

PoseBackup *ED_pose_backup_create_selected_bones(const Object *ob, const bAction *action)
{
  /* See if bone selection is relevant. */
  bool all_bones_selected = true;
  bool no_bones_selected = true;
  const bArmature *armature = ob->data;
  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    const bool is_selected = PBONE_SELECTED(armature, pchan->bone);
    all_bones_selected &= is_selected;
    no_bones_selected &= !is_selected;
  }

  /* If no bones are selected, act as if all are. */
  const bool is_bone_selection_relevant = !all_bones_selected && !no_bones_selected;
  return pose_backup_create(ob, action, is_bone_selection_relevant);
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
