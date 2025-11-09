/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_sound.h"

#include "DNA_sound_types.h"

namespace blender::deg {

SoundBackup::SoundBackup(const Depsgraph * /*depsgraph*/)
{
  reset();
}

void SoundBackup::reset()
{
  this->cache = nullptr;
  this->waveform = nullptr;
  this->playback_handle = nullptr;
}

void SoundBackup::init_from_sound(bSound *sound)
{
  BKE_sound_runtime_state_get_and_clear(
      sound, &this->cache, &this->playback_handle, &this->waveform);
}

void SoundBackup::restore_to_sound(bSound *sound)
{
  BKE_sound_runtime_state_set(sound, this->cache, this->playback_handle, this->waveform);
  reset();
}

}  // namespace blender::deg
