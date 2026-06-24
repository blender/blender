/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <memory>

/* For getting the experimental flag for remote library support. */
#include "DNA_userdef_types.h"

#include "AS_remote_library.hh"

#include "all_library.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"asset.library"};

namespace asset_system {

AllAssetLibrary::AllAssetLibrary()
    : AssetLibrary(ASSET_LIBRARY_ALL,
                   /*is_read_only=*/true)
{
}

void AllAssetLibrary::force_remote_listing_download() const
{
  /* This includes the online essentials as a separate library, if loaded. */
  AssetLibrary::foreach_loaded(
      [&](AssetLibrary &nested) {
        const std::optional<StringRefNull> url = nested.remote_url();
        if (url.has_value()) {
          remote_library_request_download(RemoteLibraryDefinitionRef{*url, nested.root_path()});
        }
      },
      /*include_all_library=*/false);
}

std::optional<AssetLibraryReference> AllAssetLibrary::library_reference() const
{
  return all_library_reference();
}

std::optional<eAssetImportMethod> AllAssetLibrary::import_method() const
{
  return {};
}

void AllAssetLibrary::rebuild_catalogs_from_nested(const bool reload_nested_catalogs)
{
  /* Only one thread should rebuild at a time. If another thread is already rebuilding, wait for it
   * to finish and then skip rebuilding. The result would effectively be the same, so re-running
   * would just be wasted work. Waiting (rather than returning early) ensures callers don't see
   * partially rebuilt catalogs. */
  std::unique_lock rebuild_lock{rebuild_mutex_, std::try_to_lock};
  if (!rebuild_lock.owns_lock()) {
    /* Another thread holds the lock and is rebuilding. Block until it is done, then return. */
    rebuild_lock.lock();
    return;
  }

  /* Start with empty catalog storage. Don't do this directly in #this.catalog_service to avoid
   * race conditions. Rather build into a new service and replace the current one when done. */
  std::unique_ptr<AssetCatalogService> new_catalog_service = std::make_unique<AssetCatalogService>(
      AssetCatalogService::read_only_tag());

  const bool skip_remote_libraries = !USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries);

  AssetLibrary::foreach_loaded(
      [&](AssetLibrary &nested) {
        const bool is_online_lib = nested.remote_url().has_value();
        if (is_online_lib && skip_remote_libraries) {
          return;
        }

        if (reload_nested_catalogs) {
          nested.catalog_service().reload_catalogs();
        }

        new_catalog_service->add_from_existing(
            nested.catalog_service(),
            /*on_duplicate_items=*/[](const AssetCatalog &existing,
                                      const AssetCatalog &to_be_ignored) {
              if (existing.path == to_be_ignored.path) {
                CLOG_DEBUG(&LOG,
                           "multiple definitions of catalog %s (path: %s), ignoring duplicate",
                           existing.catalog_id.str().c_str(),
                           existing.path.c_str());
              }
              else {
                /* This is bound to happen at some point, for example with the Online Essentials
                 * catalogs diverging from this Blender version's bundled Essentials catalogs. */
                CLOG_INFO(&LOG,
                          "multiple definitions of catalog %s with differing paths (%s vs. %s), "
                          "ignoring second one",
                          existing.catalog_id.str().c_str(),
                          existing.path.c_str(),
                          to_be_ignored.path.c_str());
              }
            });
      },
      false);

  std::lock_guard lock{catalog_service_mutex_};
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
  this->rebuild_catalogs_from_nested(/*reload_nested_catalogs=*/true);
}

}  // namespace asset_system

}  // namespace blender
