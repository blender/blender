/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
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
