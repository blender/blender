/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BKE_modifier.h"

struct ModifierData;

namespace blender::deg {

class ModifierDataBackup {
 public:
  explicit ModifierDataBackup(ModifierData *modifier_data);

  ModifierType type;
  void *runtime;
};

}  // namespace blender::deg
