/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_modifier.h"

#include "DNA_modifier_types.h"

namespace blender::deg {

ModifierDataBackup::ModifierDataBackup(ModifierData *modifier_data)
    : type(static_cast<ModifierType>(modifier_data->type)), runtime(modifier_data->runtime)
{
}

}  // namespace blender::deg
