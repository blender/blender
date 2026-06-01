/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "AS_remote_library.hh"

#include "BKE_appdir.hh"

#include "BLI_path_utils.hh"
#include "BLI_string_ref.hh"

#include "CLG_log.h"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "on_disk_library.hh"
#include "remote_library.hh"
#include "utils.hh"

#include "AS_essentials_library.hh"
#include "essentials_library.hh"

namespace blender::asset_system {

static CLG_LogRef LOG = {"asset.library.essentials"};

EssentialsAssetLibrary::EssentialsAssetLibrary()
    : OnDiskAssetLibrary(ASSET_LIBRARY_ESSENTIALS,
                         {},
                         utils::normalize_directory_path(essentials_directory_path()),
                         /*is_read_only=*/true)
{
}

void EssentialsAssetLibrary::force_remote_listing_download() const
{
  remote_library_request_download(RemoteLibraryDefinitionRef{
      online_essentials_url(), online_essentials_cache_directory_path()});
}

std::optional<AssetLibraryReference> EssentialsAssetLibrary::library_reference() const
{
  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = -1;
  library_ref.type = ASSET_LIBRARY_ESSENTIALS;
  return library_ref;
}

std::optional<eAssetImportMethod> EssentialsAssetLibrary::import_method() const
{
  if (U.experimental.no_data_block_packing) {
    return ASSET_IMPORT_APPEND_REUSE;
  }
  return ASSET_IMPORT_PACK;
}

void EssentialsAssetLibrary::refresh_catalogs()
{
  /* Start with empty catalog storage. Don't do this directly in #this.catalog_service to avoid
   * race conditions. Rather build into a new service and replace the current one when done. */
  std::unique_ptr<AssetCatalogService> new_catalog_service = std::make_unique<AssetCatalogService>(
      AssetCatalogService::read_only_tag());

  const bool skip_remote_libraries = !USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries);

  const auto load_catalogs_fn = [&](const AssetLibrary *library) {
    const bool is_online_lib = library->remote_url().has_value();
    if (is_online_lib && skip_remote_libraries) {
      return;
    }

    library->catalog_service().reload_catalogs();

    new_catalog_service->add_from_existing(
        library->catalog_service(),
        /*on_duplicate_items=*/[](const AssetCatalog &existing,
                                  const AssetCatalog &to_be_ignored) {
          if (existing.path == to_be_ignored.path) {
            CLOG_DEBUG(&LOG,
                       "multiple definitions of catalog %s (path: %s), ignoring duplicate",
                       existing.catalog_id.str().c_str(),
                       existing.path.c_str());
          }
          else {
            /* This is to be expected at some point in the future. The Online Essentials library
             * may change its catalog paths, while whatever version of Blender is running right now
             * still has the same old bundled assets. This means the Bundled Essentials and Online
             * Essentials diverge. There is no need to bother users with this, as it's bound to
             * happen eventually.
             *
             * Note that this same check happens in the 'All' library as well, and that already
             * logs this at INFO level, so there really is no need to be louder than DEBUG here. */
            CLOG_DEBUG(&LOG,
                       "multiple definitions of catalog %s with differing paths (%s vs. %s), "
                       "ignoring second one",
                       existing.catalog_id.str().c_str(),
                       existing.path.c_str(),
                       to_be_ignored.path.c_str());
          }
        });
  };

  load_catalogs_fn(this);
  load_catalogs_fn(AS_asset_library_load(nullptr, online_essentials_library_reference()));

  std::lock_guard lock{catalog_service_mutex_};
  catalog_service_ = std::move(new_catalog_service);
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

bool skip_experimental_asset_catalog(const UUID &catalog_id)
{
  /* Return true when the catalog_id should be rejected based on experimental features:
   *
   * const UUID UUID_my_feature_catalog_id("11111111-2222-3333-4444-555555555555");
   * if (!U.experimental.use_my_feature && catalog_id == UUID_my_feature_catalog_id) {
   *   return true;
   * }
   */

  /* Enable catalog for hair dynamics only if the feature is enabled. */
  const UUID UUID_hair_dynamics("df62a3e8-fc21-457b-9415-89f89af431ac");
  if (!U.experimental.use_geometry_nodes_hair_dynamics && catalog_id == UUID_hair_dynamics) {
    return true;
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name Online Essentials Library
 *
 * Internally this is a separate library. To the user, it's part of the normal Essentials library.
 * \{ */

StringRefNull online_essentials_cache_directory_path()
{
  static std::string path = []() {
    return remote_library_cache_directory_path("online-essentials");
  }();
  return path;
}

StringRefNull online_essentials_url()
{
  return OnlineEssentialsLibrary::URL;
}

bool is_online_essentials_url(const StringRef url)
{
  if (url.is_empty()) {
    return false;
  }

  if (remote_library_url_ends_with_top_meta_file_name(url)) {
    BLI_assert(url.drop_suffix(REMOTE_LIBRARY_TOP_META_FILE_NAME.size()).back() == '/');
    return url.drop_suffix(REMOTE_LIBRARY_TOP_META_FILE_NAME.size()) ==
           OnlineEssentialsLibrary::URL;
  }

  return url == OnlineEssentialsLibrary::URL;
}

bool is_online_essentials_dirpath(StringRef dirpath)
{
  if (dirpath.is_empty()) {
    return false;
  }
  if (dirpath.endswith(SEP_STR)) {
    dirpath = dirpath.drop_known_suffix(SEP_STR);
  }
  BLI_assert(!online_essentials_cache_directory_path().endswith(SEP_STR));

  return dirpath == online_essentials_cache_directory_path();
}

OnlineEssentialsLibrary::OnlineEssentialsLibrary()
    : RemoteAssetLibrary(ASSET_LIBRARY_ONLINE_ESSENTIALS,
                         /*is_read_only=*/true,
                         /*remote_url=*/URL,
                         /*name=*/"Online Essentials",
                         /*root_path=*/online_essentials_cache_directory_path())
{
}

std::optional<AssetLibraryReference> OnlineEssentialsLibrary::library_reference() const
{
  AssetLibraryReference library_ref{};
  library_ref.type = ASSET_LIBRARY_ONLINE_ESSENTIALS;
  library_ref.custom_library_index = -1;
  return library_ref;
}

/** \} */

}  // namespace blender::asset_system
