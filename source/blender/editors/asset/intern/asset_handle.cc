/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <string>

#include "AS_asset_representation.hh"

#include "BKE_blendfile.hh"

#include "BLI_string.h"

#include "DNA_space_types.h"

#include "DNA_space_types.h"

#include "ED_fileselect.hh"

#include "RNA_prototypes.h"

#include "ED_asset_handle.h"

blender::asset_system::AssetRepresentation *ED_asset_handle_get_representation(
    const AssetHandle *asset)
{
  return asset->file_data->asset;
}

int ED_asset_handle_get_preview_icon_id(const AssetHandle *asset)
{
  return asset->file_data->preview_icon_id;
}

int ED_asset_handle_get_preview_or_type_icon_id(const AssetHandle *asset)
{
  return ED_file_icon(asset->file_data);
}
