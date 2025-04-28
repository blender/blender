/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "asset_catalog_collection.hh"
#include "asset_catalog_definition_file.hh"

#include "on_disk_library.hh"

namespace blender::asset_system {

OnDiskAssetLibrary::OnDiskAssetLibrary(eAssetLibraryType library_type,
                                       StringRef name,
                                       StringRef root_path)
    : AssetLibrary(library_type, name, root_path)
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

void OnDiskAssetLibrary::refresh_catalogs()
{
  this->catalog_service().reload_catalogs();
}

void OnDiskAssetLibrary::load_catalogs()
{
  auto catalog_service = std::make_unique<AssetCatalogService>(root_path());
  catalog_service->load_from_disk();
  std::lock_guard lock{catalog_service_mutex_};
  catalog_service_ = std::move(catalog_service);
}

}  // namespace blender::asset_system
