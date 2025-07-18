/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_map.hh"

struct Strip;
struct StripModifierData;

namespace blender::deg {

struct Depsgraph;

class StripModifierDataBackup {
 public:
  StripModifierDataBackup();

  void reset();

  void init_from_modifier(StripModifierData *smd);
  void restore_to_modifier(StripModifierData *smd);

  bool isEmpty() const;

  void *sound_in;
  void *sound_out;
  float *last_buf;
};

/* Backup of a single strip. */
class StripBackup {
 public:
  StripBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_strip(Strip *strip);
  void restore_to_strip(Strip *strip);

  bool isEmpty() const;

  void *scene_sound;
  ListBase anims;
  Map<int, StripModifierDataBackup> modifiers;
};

}  // namespace blender::deg
