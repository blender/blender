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

#include "intern/eval/deg_eval_runtime_backup_sound.h"

#include "BLI_utildefines.h"

#include "DNA_sound_types.h"

namespace blender {
namespace deg {

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

}  // namespace deg
}  // namespace blender
