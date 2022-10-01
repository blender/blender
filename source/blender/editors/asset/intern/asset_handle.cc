/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <string>

#include "DNA_space_types.h"

#include "BLO_readfile.h"

#include "ED_asset_handle.h"
#include "ED_asset_list.hh"

#include "WM_api.h"

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

namespace blender::ed::asset {

ID *get_local_id_from_asset_or_append_and_reuse(Main &bmain,
                                                const AssetLibraryReference &library_ref,
                                                const AssetHandle asset)
{
  if (ID *local_id = ED_asset_handle_get_local_id(&asset)) {
    return local_id;
  }

  char blend_path[FILE_MAX_LIBEXTRA];
  ED_asset_handle_get_full_library_path(nullptr, &library_ref, &asset, blend_path);
  const char *id_name = ED_asset_handle_get_name(&asset);

  return WM_file_append_datablock(&bmain,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  blend_path,
                                  ED_asset_handle_get_id_type(&asset),
                                  id_name,
                                  BLO_LIBLINK_APPEND_RECURSIVE |
                                      BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR |
                                      BLO_LIBLINK_APPEND_LOCAL_ID_REUSE);
}

}  // namespace blender::ed::asset
