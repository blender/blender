/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/eval/deg_eval_runtime_backup_sequencer.h"

struct Scene;

namespace blender::deg {

struct Depsgraph;

/* Backup of scene runtime data. */
class SceneBackup {
 public:
  SceneBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_scene(Scene *scene);
  void restore_to_scene(Scene *scene);

  /* Sound/audio related pointers of the scene itself.
   *
   * NOTE: Scene can not disappear after relations update, because otherwise the entire dependency
   * graph will be gone. This means we don't need to compare original scene pointer, or worry about
   * freeing those if they can't be restored: we just copy them over to a new scene. */
  void *sound_scene;
  void *playback_handle;
  void *sound_scrub_handle;
  void *speaker_handles;
  float rigidbody_last_time;

  SequencerBackup sequencer_backup;
};

}  // namespace blender::deg
