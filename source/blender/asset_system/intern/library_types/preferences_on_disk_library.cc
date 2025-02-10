/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"

#include "DNA_userdef_types.h"

#include "preferences_on_disk_library.hh"

namespace blender::asset_system {

PreferencesOnDiskAssetLibrary::PreferencesOnDiskAssetLibrary(StringRef name, StringRef root_path)
    : OnDiskAssetLibrary(ASSET_LIBRARY_CUSTOM, name, root_path)
{
}

std::optional<AssetLibraryReference> PreferencesOnDiskAssetLibrary::library_reference() const
{
  int i;
  LISTBASE_FOREACH_INDEX (const bUserAssetLibrary *, asset_library, &U.asset_libraries, i) {
    if (!BLI_is_dir(asset_library->dirpath)) {
      continue;
    }

    if (BLI_path_cmp_normalized(asset_library->dirpath, this->root_path().c_str()) == 0) {
      AssetLibraryReference library_ref{};
      library_ref.type = ASSET_LIBRARY_CUSTOM;
      library_ref.custom_library_index = i;
      return library_ref;
    }
  }

  return {};
}

}  // namespace blender::asset_system
