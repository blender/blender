/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_library.hh"

#include "AS_asset_catalog.hh"

#include "BKE_main.hh"

#include "BLI_string_utils.hh"

#include "RNA_access.hh"

#include "ED_asset_catalog.hh"

#include "WM_api.hh"

namespace blender::ed::asset {

using namespace blender::asset_system;

bool catalogs_read_only(const AssetLibrary &library)
{
  const asset_system::AssetCatalogService &catalog_service = library.catalog_service();
  return catalog_service.is_read_only();
}

static std::string catalog_name_ensure_unique(AssetCatalogService &catalog_service,
                                              StringRefNull name,
                                              StringRef parent_path)
{
  char unique_name[MAX_NAME] = "";
  BLI_uniquename_cb(
      [&](const StringRef check_name) {
        AssetCatalogPath fullpath = AssetCatalogPath(parent_path) / check_name;
        return catalog_service.find_catalog_by_path(fullpath);
      },
      name.c_str(),
      '.',
      unique_name,
      sizeof(unique_name));

  return unique_name;
}

asset_system::AssetCatalog *catalog_add(AssetLibrary *library,
                                        StringRefNull name,
                                        StringRef parent_path)
{
  asset_system::AssetCatalogService &catalog_service = library->catalog_service();
  if (catalog_service.is_read_only()) {
    return nullptr;
  }

  std::string unique_name = catalog_name_ensure_unique(catalog_service, name, parent_path);
  AssetCatalogPath fullpath = AssetCatalogPath(parent_path) / unique_name;

  catalog_service.undo_push();
  asset_system::AssetCatalog *new_catalog = catalog_service.create_catalog(fullpath);
  if (!new_catalog) {
    return nullptr;
  }
  catalog_service.tag_has_unsaved_changes(new_catalog);

  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  return new_catalog;
}

void catalog_remove(AssetLibrary *library, const CatalogID &catalog_id)
{
  asset_system::AssetCatalogService &catalog_service = library->catalog_service();
  if (catalog_service.is_read_only()) {
    return;
  }

  catalog_service.undo_push();
  catalog_service.tag_has_unsaved_changes(nullptr);
  catalog_service.prune_catalogs_by_id(catalog_id);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

void catalog_rename(AssetLibrary *library,
                    const CatalogID catalog_id,
                    const StringRefNull new_name)
{
  asset_system::AssetCatalogService &catalog_service = library->catalog_service();
  if (catalog_service.is_read_only()) {
    return;
  }

  AssetCatalog *catalog = catalog_service.find_catalog(catalog_id);

  const AssetCatalogPath new_path = catalog->path.parent() / StringRef(new_name);
  const AssetCatalogPath clean_new_path = new_path.cleanup();

  if (new_path == catalog->path || clean_new_path == catalog->path) {
    /* Nothing changed, so don't bother renaming for nothing. */
    return;
  }

  catalog_service.undo_push();
  catalog_service.tag_has_unsaved_changes(catalog);
  catalog_service.update_catalog_path(catalog_id, clean_new_path);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

void catalog_move(AssetLibrary *library,
                  const CatalogID src_catalog_id,
                  const std::optional<CatalogID> dst_parent_catalog_id)
{
  asset_system::AssetCatalogService &catalog_service = library->catalog_service();
  if (catalog_service.is_read_only()) {
    return;
  }

  AssetCatalog *src_catalog = catalog_service.find_catalog(src_catalog_id);
  if (!src_catalog) {
    BLI_assert_unreachable();
    return;
  }
  AssetCatalog *dst_catalog = dst_parent_catalog_id ?
                                  catalog_service.find_catalog(*dst_parent_catalog_id) :
                                  nullptr;
  if (!dst_catalog && dst_parent_catalog_id) {
    BLI_assert_unreachable();
    return;
  }

  std::string unique_name = catalog_name_ensure_unique(
      catalog_service, src_catalog->path.name(), dst_catalog ? dst_catalog->path.c_str() : "");
  /* If a destination catalog was given, construct the path using that. Otherwise, the path is just
   * the name of the catalog to be moved, which means it ends up at the root level. */
  const AssetCatalogPath new_path = dst_catalog ? (dst_catalog->path / unique_name) :
                                                  AssetCatalogPath{unique_name};
  const AssetCatalogPath clean_new_path = new_path.cleanup();

  if (new_path == src_catalog->path || clean_new_path == src_catalog->path) {
    /* Nothing changed, so don't bother renaming for nothing. */
    return;
  }

  catalog_service.undo_push();
  catalog_service.tag_has_unsaved_changes(src_catalog);
  catalog_service.update_catalog_path(src_catalog_id, clean_new_path);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

void catalogs_save_from_main_path(AssetLibrary *library, const Main *bmain)
{
  asset_system::AssetCatalogService &catalog_service = library->catalog_service();
  if (catalog_service.is_read_only()) {
    return;
  }

  /* Since writing to disk also means loading any on-disk changes, it may be a good idea to store
   * an undo step. */
  catalog_service.undo_push();
  catalog_service.write_to_disk(bmain->filepath);
}

void catalogs_save_from_asset_reference(AssetLibrary &library, const AssetWeakReference &reference)
{
  asset_system::AssetCatalogService &catalog_service = library.catalog_service();
  if (catalog_service.is_read_only()) {
    return;
  }

  char asset_full_path_buffer[1024 + MAX_ID_NAME /*FILE_MAX_LIBEXTRA*/];
  char *file_path = nullptr;
  AS_asset_full_path_explode_from_weak_ref(
      &reference, asset_full_path_buffer, &file_path, nullptr, nullptr);
  if (!file_path) {
    BLI_assert_unreachable();
    return;
  }

  /* Since writing to disk also means loading any on-disk changes, it may be a good idea to store
   * an undo step. */
  catalog_service.undo_push();
  catalog_service.write_to_disk(file_path);
}

void catalogs_set_save_catalogs_when_file_is_saved(const bool should_save)
{
  asset_system::AssetLibrary::save_catalogs_when_file_is_saved = should_save;
}

bool catalogs_get_save_catalogs_when_file_is_saved()
{
  return asset_system::AssetLibrary::save_catalogs_when_file_is_saved;
}

}  // namespace blender::ed::asset
