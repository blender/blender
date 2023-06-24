/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_geom.h"

#include "BKE_lib_id.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_geom.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_cachefile_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h" /* for FILE_MAX */

namespace blender::io::usd {

void USDGeomReader::apply_cache_file(CacheFile *cache_file)
{
  if (!cache_file) {
    return;
  }

  if (needs_cachefile_ && object_) {
    ModifierData *md = BKE_modifier_new(eModifierType_MeshSequenceCache);
    BLI_addtail(&object_->modifiers, md);

    MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

    mcmd->cache_file = cache_file;
    id_us_plus(&mcmd->cache_file->id);
    mcmd->read_flag = import_params_.mesh_read_flag;

    BLI_strncpy(mcmd->object_path, prim_.GetPath().GetString().c_str(), FILE_MAX);
  }

  if (USDXformReader::needs_cachefile()) {
    USDXformReader::apply_cache_file(cache_file);
  }
}

void USDGeomReader::add_cache_modifier()
{
  /* Defer creating modifiers until a cache file is provided. */
  needs_cachefile_ = true;
}

void USDGeomReader::add_subdiv_modifier()
{
  ModifierData *md = BKE_modifier_new(eModifierType_Subsurf);
  BLI_addtail(&object_->modifiers, md);
}

}  // namespace blender::io::usd
