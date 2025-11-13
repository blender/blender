/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BKE_sound.hh"

struct bSound;

namespace blender::deg {

struct Depsgraph;

/* Backup of sound datablocks runtime data. */
class SoundBackup {
 public:
  SoundBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_sound(bSound *sound);
  void restore_to_sound(bSound *sound);

  AUD_Sound *cache;
  Vector<float> *waveform;
  AUD_Sound *playback_handle;
};

}  // namespace blender::deg
