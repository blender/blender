/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_gpencil.h"
#include "intern/depsgraph.hh"

#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_update_cache_legacy.h"

#include "DNA_gpencil_legacy_types.h"

namespace blender::deg {

GPencilBackup::GPencilBackup(const Depsgraph *depsgraph) : depsgraph(depsgraph) {}

void GPencilBackup::init_from_gpencil(bGPdata * /*gpd*/) {}

void GPencilBackup::restore_to_gpencil(bGPdata *gpd)
{
  bGPdata *gpd_orig = reinterpret_cast<bGPdata *>(gpd->id.orig_id);

  /* We check for the active depsgraph here to avoid freeing the cache on the original object
   * multiple times. This free is only needed for the case where we tagged a full update in the
   * update cache and did not do an update-on-write. */
  if (depsgraph->is_active) {
    BKE_gpencil_free_update_cache(gpd_orig);
  }
  /* Doing a copy-on-write copies the update cache pointer. Make sure to reset it
   * to null as we should never use the update cache from eval data. */
  gpd->runtime.update_cache = nullptr;
  /* Make sure to update the original runtime pointers in the eval data. */
  BKE_gpencil_data_update_orig_pointers(gpd_orig, gpd);
}

}  // namespace blender::deg
