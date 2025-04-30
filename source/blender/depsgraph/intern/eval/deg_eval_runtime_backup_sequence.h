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
class StripBackup {
 public:
  StripBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_strip(Strip *strip);
  void restore_to_strip(Strip *strip);

  bool isEmpty() const;

  void *scene_sound;
  ListBase anims;
};

}  // namespace blender::deg
