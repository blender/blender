/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include <string>

#include "BLI_vector.hh"

struct ID;

namespace blender::deg {

struct Depsgraph;

class AnimationValueBackup {
 public:
  AnimationValueBackup() = default;
  AnimationValueBackup(const std::string &rna_path, int array_index, float value);

  AnimationValueBackup(const AnimationValueBackup &other) = default;
  AnimationValueBackup(AnimationValueBackup &&other) noexcept = default;

  AnimationValueBackup &operator=(const AnimationValueBackup &other) = default;
  AnimationValueBackup &operator=(AnimationValueBackup &&other) = default;

  std::string rna_path;
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

}  // namespace blender::deg
