/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BKE_scene_runtime.hh"

#include "intern/eval/deg_eval_runtime_backup_sequencer.h"

namespace blender {

struct Scene;

namespace deg {

struct Depsgraph;

/* Backup of scene runtime data. */
class SceneBackup {
 public:
  SceneBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_scene(Scene *scene);
  void restore_to_scene(Scene *scene);

  bke::SceneAudioRuntime audio_runtime;
  float rigidbody_last_time;
  SequencerBackup sequencer_backup;
};

}  // namespace deg
}  // namespace blender
