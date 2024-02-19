/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <memory>

#include "AS_asset_catalog_tree.hh"

#include "asset_library_all.hh"

namespace blender::asset_system {

AllAssetLibrary::AllAssetLibrary() : AssetLibrary(ASSET_LIBRARY_ALL) {}

void AllAssetLibrary::rebuild_catalogs_from_nested(const bool reload_nested_catalogs)
{
  /* Start with empty catalog storage. Don't do this directly in #this.catalog_service to avoid
   * race conditions. Rather build into a new service and replace the current one when done. */
  std::unique_ptr<AssetCatalogService> new_catalog_service = std::make_unique<AssetCatalogService>(
      AssetCatalogService::read_only_tag());

  AssetLibrary::foreach_loaded(
      [&](AssetLibrary &nested) {
        if (reload_nested_catalogs) {
          nested.catalog_service->reload_catalogs();
        }
        new_catalog_service->add_from_existing(*nested.catalog_service);
      },
      false);
  new_catalog_service->rebuild_tree();
  this->catalog_service = std::move(new_catalog_service);
}

void AllAssetLibrary::refresh_catalogs()
{
  rebuild_catalogs_from_nested(/*reload_nested_catalogs=*/true);
}

}  // namespace blender::asset_system
