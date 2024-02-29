/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <memory>

#include "AS_asset_catalog_tree.hh"
#include "asset_catalog_collection.hh"
#include "asset_catalog_definition_file.hh"

#include "asset_library_all.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"asset_system.all_asset_library"};

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
          nested.catalog_service().reload_catalogs();
        }

        new_catalog_service->add_from_existing(
            nested.catalog_service(),
            /*on_duplicate_items=*/[](const AssetCatalog &existing,
                                      const AssetCatalog &to_be_ignored) {
              if (existing.path == to_be_ignored.path) {
                CLOG_INFO(&LOG,
                          2,
                          "multiple definitions of catalog %s (path: %s), ignoring duplicate",
                          existing.catalog_id.str().c_str(),
                          existing.path.c_str());
              }
              else {
                CLOG_ERROR(&LOG,
                           "multiple definitions of catalog %s with differing paths (%s vs. %s), "
                           "ignoring second one",
                           existing.catalog_id.str().c_str(),
                           existing.path.c_str(),
                           to_be_ignored.path.c_str());
              }
            });
      },
      false);

  catalog_service_ = std::move(new_catalog_service);
  catalogs_dirty_ = false;
}

void AllAssetLibrary::tag_catalogs_dirty()
{
  catalogs_dirty_ = true;
}

bool AllAssetLibrary::is_catalogs_dirty() const
{
  return catalogs_dirty_;
}

void AllAssetLibrary::refresh_catalogs()
{
  rebuild_catalogs_from_nested(/*reload_nested_catalogs=*/true);
}

}  // namespace blender::asset_system
