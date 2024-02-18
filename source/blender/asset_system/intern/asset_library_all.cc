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

void AllAssetLibrary::rebuild(const bool reload_catalogs)
{
  /* Start with empty catalog storage. */
  catalog_service = std::make_unique<AssetCatalogService>(AssetCatalogService::read_only_tag());

  AssetLibrary::foreach_loaded(
      [&](AssetLibrary &nested) {
        if (reload_catalogs) {
          nested.catalog_service->reload_catalogs();
        }
        catalog_service->add_from_existing(*nested.catalog_service);
      },
      false);
  catalog_service->rebuild_tree();
}

void AllAssetLibrary::refresh_catalogs()
{
  rebuild(/*reload_catalogs=*/true);
}

}  // namespace blender::asset_system
