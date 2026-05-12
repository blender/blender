/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BLI_assert.h"
#include "BLI_listbase.h"

#include "DNA_userdef_types.h"

#include "common.hh"
#include "preferences_on_disk_library.hh"

namespace blender::asset_system {

PreferencesOnDiskAssetLibrary::PreferencesOnDiskAssetLibrary(
    const bUserAssetLibrary &user_asset_library)
    : OnDiskAssetLibrary(ASSET_LIBRARY_CUSTOM,
                         user_asset_library.name,
                         user_asset_library.dirpath,
                         /*is_read_only=*/false),
      user_library_(user_asset_library)
{
}

std::optional<AssetLibraryReference> PreferencesOnDiskAssetLibrary::library_reference() const
{
  const bUserAssetLibrary *library_definition = user_library_.user_asset_library();
  if (!library_definition) {
    return {};
  }
  const int index = BLI_findindex(&U.asset_libraries, library_definition);
  if (index == -1) {
    /* Should have been caught by the #user_asset_library() call above already. */
    BLI_assert_unreachable();
    return {};
  }

  AssetLibraryReference library_ref{};
  library_ref.type = ASSET_LIBRARY_CUSTOM;
  library_ref.custom_library_index = index;
  return library_ref;
}

std::optional<eAssetImportMethod> PreferencesOnDiskAssetLibrary::import_method() const
{
  const bUserAssetLibrary *library_definition = user_library_.user_asset_library();
  if (!library_definition) {
    return {};
  }

  return eAssetImportMethod(library_definition->import_method);
}

bool PreferencesOnDiskAssetLibrary::use_relative_paths() const
{
  const bUserAssetLibrary *library_definition = user_library_.user_asset_library();
  if (!library_definition) {
    return false;
  }

  return (library_definition->flag & ASSET_LIBRARY_RELATIVE_PATH) != 0;
}

bool PreferencesOnDiskAssetLibrary::is_enabled() const
{
  const bUserAssetLibrary *library_definition = user_library_.user_asset_library();
  if (!library_definition) {
    return false;
  }

  return (library_definition->flag & ASSET_LIBRARY_DISABLED) == 0;
}

}  // namespace blender::asset_system
