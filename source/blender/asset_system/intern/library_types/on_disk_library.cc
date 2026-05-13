/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "on_disk_library.hh"

namespace blender::asset_system {

OnDiskAssetLibrary::OnDiskAssetLibrary(eAssetLibraryType library_type,
                                       StringRef name,
                                       StringRef root_path,
                                       const bool is_read_only)
    : AssetLibrary(library_type, /*is_read_only=*/is_read_only, name, root_path)
{
  this->on_blend_save_handler_register();
}

std::optional<AssetLibraryReference> OnDiskAssetLibrary::library_reference() const
{
  if (library_type() == ASSET_LIBRARY_LOCAL) {
    AssetLibraryReference library_ref{};
    library_ref.custom_library_index = -1;
    library_ref.type = ASSET_LIBRARY_LOCAL;
    return library_ref;
  }

  BLI_assert_msg(false,
                 "Library references are only available for built-in libraries and libraries "
                 "configured in the Preferences");
  return {};
}

std::optional<eAssetImportMethod> OnDiskAssetLibrary::import_method() const
{
  return {};
}

void OnDiskAssetLibrary::refresh_catalogs()
{
  this->catalog_service().reload_catalogs();
}

bool OnDiskAssetLibrary::is_enabled() const
{
  return true;
}

}  // namespace blender::asset_system
