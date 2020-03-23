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
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_scene.h"

#include "BKE_sound.h"

#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

namespace DEG {

SceneBackup::SceneBackup(const Depsgraph *depsgraph) : sequencer_backup(depsgraph)
{
  reset();
}

void SceneBackup::reset()
{
  sound_scene = nullptr;
  playback_handle = nullptr;
  sound_scrub_handle = nullptr;
  speaker_handles = nullptr;
  rigidbody_last_time = -1;
}

void SceneBackup::init_from_scene(Scene *scene)
{
  BKE_sound_lock_scene(scene);

  sound_scene = scene->sound_scene;
  playback_handle = scene->playback_handle;
  sound_scrub_handle = scene->sound_scrub_handle;
  speaker_handles = scene->speaker_handles;

  if (scene->rigidbody_world != nullptr) {
    rigidbody_last_time = scene->rigidbody_world->ltime;
  }

  /* Clear pointers stored in the scene, so they are not freed when copied-on-written datablock
   * is freed for re-allocation. */
  scene->sound_scene = nullptr;
  scene->playback_handle = nullptr;
  scene->sound_scrub_handle = nullptr;
  scene->speaker_handles = nullptr;

  sequencer_backup.init_from_scene(scene);
}

void SceneBackup::restore_to_scene(Scene *scene)
{
  scene->sound_scene = sound_scene;
  scene->playback_handle = playback_handle;
  scene->sound_scrub_handle = sound_scrub_handle;
  scene->speaker_handles = speaker_handles;

  if (scene->rigidbody_world != nullptr) {
    scene->rigidbody_world->ltime = rigidbody_last_time;
  }

  sequencer_backup.restore_to_scene(scene);

  BKE_sound_unlock_scene(scene);

  reset();
}

}  // namespace DEG
