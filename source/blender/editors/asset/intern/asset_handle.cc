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
 */

/** \file
 * \ingroup edasset
 *
 * Asset-handle is a temporary design, not part of the core asset system design.
 *
 * Currently asset-list items are just file directory items (#FileDirEntry). So an asset-handle is
 * just wraps a pointer to this. We try to abstract away the fact that it's just a file entry,
 * although that doesn't always work (see #rna_def_asset_handle()).
 */

#include <string>

#include "DNA_space_types.h"

#include "BLO_readfile.h"

#include "ED_asset_handle.h"
#include "ED_asset_list.hh"

const char *ED_asset_handle_get_name(const AssetHandle *asset)
{
  return asset->file_data->name;
}

AssetMetaData *ED_asset_handle_get_metadata(const AssetHandle *asset)
{
  return asset->file_data->asset_data;
}

ID *ED_asset_handle_get_local_id(const AssetHandle *asset)
{
  return asset->file_data->id;
}

ID_Type ED_asset_handle_get_id_type(const AssetHandle *asset)
{
  return static_cast<ID_Type>(asset->file_data->blentype);
}

int ED_asset_handle_get_preview_icon_id(const AssetHandle *asset)
{
  return asset->file_data->preview_icon_id;
}

void ED_asset_handle_get_full_library_path(const bContext *C,
                                           const AssetLibraryReference *asset_library_ref,
                                           const AssetHandle *asset,
                                           char r_full_lib_path[FILE_MAX_LIBEXTRA])
{
  *r_full_lib_path = '\0';

  std::string asset_path = ED_assetlist_asset_filepath_get(C, *asset_library_ref, *asset);
  if (asset_path.empty()) {
    return;
  }

  BLO_library_path_explode(asset_path.c_str(), r_full_lib_path, nullptr, nullptr);
}
