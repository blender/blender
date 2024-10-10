/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <string>

#include "AS_asset_representation.hh"

#include "ED_asset.hh"

namespace blender::ed::asset {

std::string asset_tooltip(const asset_system::AssetRepresentation &asset, const bool include_name)
{
  std::string complete_string;

  if (include_name) {
    complete_string += asset.get_name();
  }

  const AssetMetaData &meta_data = asset.get_metadata();
  if (meta_data.description) {
    complete_string += '\n';
    complete_string += meta_data.description;
  }
  return complete_string;
}

}  // namespace blender::ed::asset
