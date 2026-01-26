/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BKE_sound.hh"

namespace blender {

struct bSound;

namespace deg {

struct Depsgraph;

/* Backup of sound datablocks runtime data. */
class SoundBackup {
 public:
  SoundBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_sound(bSound *sound);
  void restore_to_sound(bSound *sound);

  AUD_Sound cache;
  AUD_Sound playback_handle;
  Vector<float> *waveform;
};

}  // namespace deg
}  // namespace blender
