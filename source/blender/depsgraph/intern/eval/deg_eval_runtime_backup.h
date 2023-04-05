/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "DNA_ID.h"

#include "intern/eval/deg_eval_runtime_backup_animation.h"
#include "intern/eval/deg_eval_runtime_backup_gpencil.h"
#include "intern/eval/deg_eval_runtime_backup_movieclip.h"
#include "intern/eval/deg_eval_runtime_backup_object.h"
#include "intern/eval/deg_eval_runtime_backup_scene.h"
#include "intern/eval/deg_eval_runtime_backup_sound.h"
#include "intern/eval/deg_eval_runtime_backup_volume.h"

namespace blender::deg {

struct Depsgraph;

class RuntimeBackup {
 public:
  explicit RuntimeBackup(const Depsgraph *depsgraph);

  /* NOTE: Will reset all runtime fields which has been backed up to nullptr. */
  void init_from_id(ID *id);

  /* Restore fields to the given ID. */
  void restore_to_id(ID *id);

  /* Denotes whether init_from_id did put anything into the backup storage.
   * This will not be the case when init_from_id() is called for an ID which has never been
   * copied-on-write. In this case there is no need to backup or restore anything.
   *
   * It also allows to have restore() logic to be symmetrical to init() without need to worry
   * that init() might not have happened.
   *
   * In practice this is used by audio system to lock audio while scene is going through
   * copy-on-write mechanism. */
  bool have_backup;

  /* Struct members of the ID pointer. */
  struct {
    void *py_instance;
  } id_data;

  AnimationBackup animation_backup;
  SceneBackup scene_backup;
  SoundBackup sound_backup;
  ObjectRuntimeBackup object_backup;
  DrawDataList drawdata_backup;
  DrawDataList *drawdata_ptr;
  MovieClipBackup movieclip_backup;
  VolumeBackup volume_backup;
  GPencilBackup gpencil_backup;
};

}  // namespace blender::deg
