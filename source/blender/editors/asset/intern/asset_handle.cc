/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_representation.hh"

#include "DNA_space_types.h"

#include "ED_fileselect.hh"

#include "ED_asset_handle.hh"

namespace blender::ed::asset {

asset_system::AssetRepresentation *handle_get_representation(const AssetHandle *asset)
{
  return asset->file_data->asset;
}

}  // namespace blender::ed::asset
