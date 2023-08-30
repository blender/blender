/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <string>

#include "AS_asset_representation.hh"

#include "BKE_blendfile.h"

#include "BLI_string.h"

#include "DNA_space_types.h"

#include "DNA_space_types.h"

#include "ED_fileselect.hh"

#include "RNA_prototypes.h"

#include "ED_asset_handle.h"

blender::asset_system::AssetRepresentation *ED_asset_handle_get_representation(
    const AssetHandle *asset_handle)
{
  return asset_handle->file_data->asset;
}

const char *ED_asset_handle_get_identifier(const AssetHandle *asset_handle)
{
  return asset_handle->file_data->asset->get_identifier().library_relative_identifier().c_str();
}

const char *ED_asset_handle_get_name(const AssetHandle *asset_handle)
{
  return asset_handle->file_data->asset->get_name().c_str();
}

ID_Type ED_asset_handle_get_id_type(const AssetHandle *asset_handle)
{
  return asset_handle->file_data->asset->get_id_type();
}

int ED_asset_handle_get_preview_icon_id(const AssetHandle *asset)
{
  return asset->file_data->preview_icon_id;
}

int ED_asset_handle_get_preview_or_type_icon_id(const AssetHandle *asset)
{
  return ED_file_icon(asset->file_data);
}

void ED_asset_handle_get_full_library_path(const AssetHandle *asset_handle,
                                           char r_full_lib_path[FILE_MAX])
{
  *r_full_lib_path = '\0';

  std::string library_path = asset_handle->file_data->asset->get_identifier().full_library_path();
  if (library_path.empty()) {
    return;
  }

  BLI_strncpy(r_full_lib_path, library_path.c_str(), FILE_MAX);
}

namespace blender::ed::asset {

PointerRNA create_asset_rna_ptr(const asset_system::AssetRepresentation *asset)
{
  PointerRNA ptr{};
  ptr.owner_id = nullptr;
  ptr.type = &RNA_AssetRepresentation;
  ptr.data = const_cast<asset_system::AssetRepresentation *>(asset);
  return ptr;
}

}  // namespace blender::ed::asset
