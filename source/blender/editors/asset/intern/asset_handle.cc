/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <string>

#include "AS_asset_representation.h"
#include "AS_asset_representation.hh"

#include "BKE_blendfile.h"

#include "BLI_string.h"

#include "DNA_space_types.h"

#include "DNA_space_types.h"

#include "ED_asset_handle.h"

AssetRepresentation *ED_asset_handle_get_representation(const AssetHandle *asset)
{
  return asset->file_data->asset;
}

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

blender::StringRefNull ED_asset_handle_get_relative_path(const AssetHandle &asset)
{
  return AS_asset_representation_relative_path_get(asset.file_data->asset);
}

void ED_asset_handle_get_full_library_path(const AssetHandle *asset_handle,
                                           char r_full_lib_path[FILE_MAX_LIBEXTRA])
{
  *r_full_lib_path = '\0';

  std::string library_path = AS_asset_representation_full_library_path_get(
      asset_handle->file_data->asset);
  if (library_path.empty()) {
    return;
  }

  BLI_strncpy(r_full_lib_path, library_path.c_str(), FILE_MAX_LIBEXTRA);
}

bool ED_asset_handle_get_use_relative_path(const AssetHandle *asset)
{
  return AS_asset_representation_use_relative_path_get(asset->file_data->asset);
}
