/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BLI_listbase.h"

struct Sequence;

namespace blender {
namespace deg {

struct Depsgraph;

/* Backup of a single strip. */
class SequenceBackup {
 public:
  SequenceBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_sequence(Sequence *sequence);
  void restore_to_sequence(Sequence *sequence);

  bool isEmpty() const;

  void *scene_sound;
  ListBase anims;
};

}  // namespace deg
}  // namespace blender
