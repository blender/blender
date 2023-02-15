/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <string>

#include "AS_asset_representation.h"
#include "AS_asset_representation.hh"

#include "DNA_space_types.h"

#include "BLO_readfile.h"

#include "ED_asset_handle.h"

#include "WM_api.h"

const char *ED_asset_handle_get_name(const AssetHandle *asset)
{
  return AS_asset_representation_name_get(asset->file_data->asset);
}

AssetMetaData *ED_asset_handle_get_metadata(const AssetHandle *asset_handle)
{
  return AS_asset_representation_metadata_get(asset_handle->file_data->asset);
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

std::optional<eAssetImportMethod> ED_asset_handle_get_import_method(
    const AssetHandle *asset_handle)
{
  return AS_asset_representation_import_method_get(asset_handle->file_data->asset);
}

void ED_asset_handle_get_full_library_path(const AssetHandle *asset_handle,
                                           char r_full_lib_path[FILE_MAX_LIBEXTRA])
{
  *r_full_lib_path = '\0';

  std::string asset_path = AS_asset_representation_full_path_get(asset_handle->file_data->asset);
  if (asset_path.empty()) {
    return;
  }

  BLO_library_path_explode(asset_path.c_str(), r_full_lib_path, nullptr, nullptr);
}

namespace blender::ed::asset {

ID *get_local_id_from_asset_or_append_and_reuse(Main &bmain, const AssetHandle asset)
{
  if (ID *local_id = ED_asset_handle_get_local_id(&asset)) {
    return local_id;
  }

  char blend_path[FILE_MAX_LIBEXTRA];
  ED_asset_handle_get_full_library_path(&asset, blend_path);
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
