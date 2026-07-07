/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_scene.h"

#include "BKE_scene_runtime.hh"
#include "BKE_sound.hh"

#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

namespace blender::deg {

SceneBackup::SceneBackup(const Depsgraph *depsgraph) : sequencer_backup(depsgraph)
{
  reset();
}

void SceneBackup::reset()
{
  audio_runtime = {};
  rigidbody_last_time = -1;
}

void SceneBackup::init_from_scene(Scene *scene)
{
  BKE_sound_lock();

  if (scene->rigidbody_world != nullptr) {
    rigidbody_last_time = scene->rigidbody_world->ltime;
  }

  audio_runtime = std::move(scene->runtime->audio);
  scene->runtime->audio = {};

  sequencer_backup.init_from_scene(scene);
}

void SceneBackup::restore_to_scene(Scene *scene)
{
  scene->runtime->audio = std::move(audio_runtime);

  if (scene->rigidbody_world != nullptr) {
    scene->rigidbody_world->ltime = rigidbody_last_time;
  }

  sequencer_backup.restore_to_scene(scene);

  BKE_sound_unlock();

  reset();
}

}  // namespace blender::deg
