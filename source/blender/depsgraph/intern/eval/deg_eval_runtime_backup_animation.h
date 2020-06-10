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

#pragma once

#include "BKE_modifier.h"

#include "intern/depsgraph_type.h"

namespace DEG {

struct Depsgraph;

class AnimationValueBackup {
 public:
  AnimationValueBackup();
  AnimationValueBackup(const string &rna_path, int array_index, float value);
  ~AnimationValueBackup();

  AnimationValueBackup(const AnimationValueBackup &other) = default;
  AnimationValueBackup(AnimationValueBackup &&other) noexcept = default;

  AnimationValueBackup &operator=(const AnimationValueBackup &other) = default;
  AnimationValueBackup &operator=(AnimationValueBackup &&other) = default;

  string rna_path;
  int array_index;
  float value;
};

/* Backup of animated properties values. */
class AnimationBackup {
 public:
  AnimationBackup(const Depsgraph *depsgraph);

  void reset();

  void init_from_id(ID *id);
  void restore_to_id(ID *id);

  bool meed_value_backup;
  Vector<AnimationValueBackup> values_backup;
};

}  // namespace DEG
