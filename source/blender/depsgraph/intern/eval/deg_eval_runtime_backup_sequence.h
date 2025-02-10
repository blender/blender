/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "DNA_listBase.h"

struct Strip;

namespace blender::deg {

struct Depsgraph;

/* Backup of a single strip. */
class SequenceBackup {
 public:
  SequenceBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_sequence(Strip *sequence);
  void restore_to_sequence(Strip *sequence);

  bool isEmpty() const;

  void *scene_sound;
  ListBase anims;
};

}  // namespace blender::deg
