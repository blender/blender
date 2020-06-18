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

struct ModifierData;

namespace DEG {

struct Depsgraph;

/* Identifier used to match modifiers to backup/restore their runtime data.
 * Identification is happening using original modifier data pointer and the
 * modifier type.
 * It is not enough to only pointer, since it's possible to have a situation
 * when modifier is removed and a new one added, and due to memory allocation
 * policy they might have same pointer.
 * By adding type into matching we are at least ensuring that modifier will not
 * try to interpret runtime data created by another modifier type. */
class ModifierDataBackupID {
 public:
  ModifierDataBackupID(const Depsgraph *depsgraph);
  ModifierDataBackupID(ModifierData *modifier_data, ModifierType type);

  friend bool operator==(const ModifierDataBackupID &a, const ModifierDataBackupID &b);

  uint32_t hash() const;

  ModifierData *modifier_data;
  ModifierType type;
};

/* Storage for backed up runtime modifier data. */
typedef Map<ModifierDataBackupID, void *> ModifierRuntimeDataBackup;

}  // namespace DEG
