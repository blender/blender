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

ID *ED_asset_handle_get_local_id(const AssetHandle *asset_handle)
{
  return AS_asset_representation_local_id_get(asset_handle->file_data->asset);
}

ID_Type ED_asset_handle_get_id_type(const AssetHandle *asset_handle)
{
  return AS_asset_representation_id_type_get(asset_handle->file_data->asset);
}

int ED_asset_handle_get_preview_icon_id(const AssetHandle *asset)
{
  return asset->file_data->preview_icon_id;
}

void ED_asset_handle_get_full_library_path(const AssetHandle *asset_handle,
                                           char r_full_lib_path[FILE_MAX])
{
  *r_full_lib_path = '\0';

  std::string library_path = AS_asset_representation_full_library_path_get(
      asset_handle->file_data->asset);
  if (library_path.empty()) {
    return;
  }

  BLI_strncpy(r_full_lib_path, library_path.c_str(), FILE_MAX);
}
