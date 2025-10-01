/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BKE_appdir.hh"

#include "utils.hh"

#include "DNA_userdef_types.h"

#include "AS_essentials_library.hh"
#include "essentials_library.hh"

namespace blender::asset_system {

EssentialsAssetLibrary::EssentialsAssetLibrary()
    : OnDiskAssetLibrary(ASSET_LIBRARY_ESSENTIALS,
                         {},
                         utils::normalize_directory_path(essentials_directory_path()))
{
  import_method_ = ASSET_IMPORT_PACK;
  if (U.experimental.no_data_block_packing) {
    import_method_ = ASSET_IMPORT_APPEND_REUSE;
  }
}

std::optional<AssetLibraryReference> EssentialsAssetLibrary::library_reference() const
{
  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = -1;
  library_ref.type = ASSET_LIBRARY_ESSENTIALS;
  return library_ref;
}

void EssentialsAssetLibrary::update_default_import_method()
{
  import_method_ = ASSET_IMPORT_PACK;
  if (U.experimental.no_data_block_packing) {
    import_method_ = ASSET_IMPORT_APPEND_REUSE;
  }
}

StringRefNull essentials_directory_path()
{
  static std::string path = []() {
    const std::optional<std::string> datafiles_path = BKE_appdir_folder_id(
        BLENDER_SYSTEM_DATAFILES, "assets");
    return datafiles_path.value_or("");
  }();
  return path;
}

}  // namespace blender::asset_system
