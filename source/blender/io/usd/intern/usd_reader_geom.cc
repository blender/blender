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
 * The Original Code is Copyright (C) 2021 Tangent Animation.
 * All rights reserved.
 */

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

void USDGeomReader::add_cache_modifier()
{
  ModifierData *md = BKE_modifier_new(eModifierType_MeshSequenceCache);
  BLI_addtail(&object_->modifiers, md);

  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  mcmd->cache_file = settings_->cache_file;
  id_us_plus(&mcmd->cache_file->id);
  mcmd->read_flag = import_params_.mesh_read_flag;

  BLI_strncpy(mcmd->object_path, prim_.GetPath().GetString().c_str(), FILE_MAX);
}

void USDGeomReader::add_subdiv_modifier()
{
  ModifierData *md = BKE_modifier_new(eModifierType_Subsurf);
  BLI_addtail(&object_->modifiers, md);
}

}  // namespace blender::io::usd
