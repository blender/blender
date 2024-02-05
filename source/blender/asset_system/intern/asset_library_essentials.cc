/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BKE_appdir.hh"

#include "utils.hh"

#include "AS_essentials_library.hh"
#include "asset_library_essentials.hh"

namespace blender::asset_system {

EssentialsAssetLibrary::EssentialsAssetLibrary()
    : OnDiskAssetLibrary(ASSET_LIBRARY_ESSENTIALS,
                         {},
                         utils::normalize_directory_path(essentials_directory_path()))
{
  import_method_ = ASSET_IMPORT_APPEND_REUSE;
}

StringRefNull essentials_directory_path()
{
  static std::string path = []() {
    const std::optional<std::string> datafiles_path = BKE_appdir_folder_id(BLENDER_DATAFILES,
                                                                           "assets");
    return datafiles_path.value_or("");
  }();
  return path;
}

}  // namespace blender::asset_system
