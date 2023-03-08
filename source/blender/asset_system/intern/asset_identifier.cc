/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <iostream>

#include "BLI_path_util.h"

#include "AS_asset_identifier.hh"

namespace blender::asset_system {

AssetIdentifier::AssetIdentifier(std::shared_ptr<std::string> library_root_path,
                                 std::string relative_asset_path)
    : library_root_path_(library_root_path), relative_asset_path_(relative_asset_path)
{
}

StringRefNull AssetIdentifier::library_relative_identifier() const
{
  return relative_asset_path_;
}

std::string AssetIdentifier::full_path() const
{
  char path[FILE_MAX];
  BLI_path_join(path, sizeof(path), library_root_path_->c_str(), relative_asset_path_.c_str());
  return path;
}

}  // namespace blender::asset_system
