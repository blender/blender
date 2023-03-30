/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_volume.h"

#include "BLI_assert.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_volume_types.h"

#include "BKE_volume.h"

#include <cstdio>

namespace blender::deg {

VolumeBackup::VolumeBackup(const Depsgraph * /*depsgraph*/) : grids(nullptr) {}

void VolumeBackup::init_from_volume(Volume *volume)
{
  STRNCPY(filepath, volume->filepath);
  BLI_STATIC_ASSERT(sizeof(filepath) == sizeof(volume->filepath),
                    "VolumeBackup filepath length wrong");

  grids = volume->runtime.grids;
  volume->runtime.grids = nullptr;
}

void VolumeBackup::restore_to_volume(Volume *volume)
{
  if (grids) {
    BKE_volume_grids_backup_restore(volume, grids, filepath);
    grids = nullptr;
  }
}

}  // namespace blender::deg
