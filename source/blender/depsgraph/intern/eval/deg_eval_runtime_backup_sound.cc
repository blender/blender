/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_sound.h"

#include "BLI_utildefines.h"

#include "DNA_sound_types.h"

namespace blender::deg {

SoundBackup::SoundBackup(const Depsgraph * /*depsgraph*/)
{
  reset();
}

void SoundBackup::reset()
{
  cache = nullptr;
  waveform = nullptr;
  playback_handle = nullptr;
}

void SoundBackup::init_from_sound(bSound *sound)
{
  cache = sound->cache;
  waveform = sound->waveform;
  playback_handle = sound->playback_handle;

  sound->cache = nullptr;
  sound->waveform = nullptr;
  sound->playback_handle = nullptr;
}

void SoundBackup::restore_to_sound(bSound *sound)
{
  sound->cache = cache;
  sound->waveform = waveform;
  sound->playback_handle = playback_handle;

  reset();
}

}  // namespace blender::deg
