/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Pose Backups can be created from the current pose, and later restored. The
 * backup is restricted to those bones animated by a given Action, so that
 * operations are as fast as possible.
 */

#pragma once

#include <stdbool.h>

#include "BLI_listbase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PoseBackup;

/**
 * Create a backup of those bones that are selected AND animated in the given action.
 *
 * The backup is owned by the caller, and should be freed with `BKE_pose_backup_free()`.
 */
struct PoseBackup *BKE_pose_backup_create_selected_bones(
    const struct Object *ob, const struct bAction *action) ATTR_WARN_UNUSED_RESULT;

/**
 * Create a backup of those bones that are animated in the given action.
 *
 * The backup is owned by the caller, and should be freed with `BKE_pose_backup_free()`.
 */
struct PoseBackup *BKE_pose_backup_create_all_bones(
    const struct Object *ob, const struct bAction *action) ATTR_WARN_UNUSED_RESULT;
bool BKE_pose_backup_is_selection_relevant(const struct PoseBackup *pose_backup);
void BKE_pose_backup_restore(const struct PoseBackup *pbd);
void BKE_pose_backup_free(struct PoseBackup *pbd);

/**
 * Create a backup of those bones that are animated in the given action.
 *
 * The backup is owned by the Object, and there can be only one backup at a time.
 * It should be freed with `BKE_pose_backup_clear(ob)`.
 */
void BKE_pose_backup_create_on_object(struct Object *ob, const struct bAction *action);

/**
 * Restore the pose backup owned by this OBject.
 *
 * \return true on success, false if there was no pose backup to restore.
 *
 * \see #BKE_pose_backup_create_on_object
 */
bool BKE_pose_backup_restore_on_object(struct Object *ob);

/**
 * Free the pose backup that was stored on this object's runtime data.
 */
void BKE_pose_backup_clear(struct Object *ob);

#ifdef __cplusplus
}
#endif
