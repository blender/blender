/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_geom.hh"

#include "BKE_lib_id.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_cachefile_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h" /* for FILE_MAX */

namespace blender::io::usd {

void USDGeomReader::add_cache_modifier()
{
  if (!settings_->get_cache_file) {
    return;
  }

  ModifierData *md = BKE_modifier_new(eModifierType_MeshSequenceCache);
  BLI_addtail(&object_->modifiers, md);
  BKE_modifiers_persistent_uid_init(*object_, *md);

  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  mcmd->cache_file = settings_->get_cache_file();
  id_us_plus(&mcmd->cache_file->id);
  mcmd->read_flag = import_params_.mesh_read_flag;

  STRNCPY(mcmd->object_path, prim_.GetPath().GetString().c_str());
}

void USDGeomReader::add_subdiv_modifier()
{
  ModifierData *md = BKE_modifier_new(eModifierType_Subsurf);
  BLI_addtail(&object_->modifiers, md);
  BKE_modifiers_persistent_uid_init(*object_, *md);
}

}  // namespace blender::io::usd
