/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "SEQ_modifier.hh"

namespace blender {

struct MovieReader;
struct Strip;
struct StripModifierData;

namespace deg {

struct Depsgraph;

class StripModifierDataBackup {
 public:
  StripModifierDataBackup();

  void reset();

  void init_from_modifier(StripModifierData *smd);
  void restore_to_modifier(StripModifierData *smd);

  bool isEmpty() const;

  /* For Sound Modifiers. */
  void *sound_in;
  void *sound_out;
  eStripModifierFlag flag;
  uint64_t params_hash;
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
  Vector<MovieReader *, 1> movie_readers;
  Map<int, StripModifierDataBackup> modifiers;
};

}  // namespace deg
}  // namespace blender
