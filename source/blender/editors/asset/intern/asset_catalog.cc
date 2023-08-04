/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_library.hh"

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"

#include "BKE_main.h"

#include "BLI_string_utils.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "ED_asset_catalog.h"
#include "ED_asset_catalog.hh"

#include "WM_api.hh"

using namespace blender;
using namespace blender::asset_system;

bool ED_asset_catalogs_read_only(const ::AssetLibrary &library)
{
  asset_system::AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(
      &library);
  return catalog_service->is_read_only();
}

struct CatalogUniqueNameFnData {
  const AssetCatalogService &catalog_service;
  StringRef parent_path;
};

static bool catalog_name_exists_fn(void *arg, const char *name)
{
  CatalogUniqueNameFnData &fn_data = *static_cast<CatalogUniqueNameFnData *>(arg);
  AssetCatalogPath fullpath = AssetCatalogPath(fn_data.parent_path) / name;
  return fn_data.catalog_service.find_catalog_by_path(fullpath);
}

static std::string catalog_name_ensure_unique(AssetCatalogService &catalog_service,
                                              StringRefNull name,
                                              StringRef parent_path)
{
  CatalogUniqueNameFnData fn_data = {catalog_service, parent_path};

  char unique_name[MAX_NAME] = "";
  BLI_uniquename_cb(
      catalog_name_exists_fn, &fn_data, name.c_str(), '.', unique_name, sizeof(unique_name));

  return unique_name;
}

asset_system::AssetCatalog *ED_asset_catalog_add(::AssetLibrary *library,
                                                 StringRefNull name,
                                                 StringRef parent_path)
{
  asset_system::AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(
      library);
  if (!catalog_service) {
    return nullptr;
  }
  if (ED_asset_catalogs_read_only(*library)) {
    return nullptr;
  }

  std::string unique_name = catalog_name_ensure_unique(*catalog_service, name, parent_path);
  AssetCatalogPath fullpath = AssetCatalogPath(parent_path) / unique_name;

  catalog_service->undo_push();
  asset_system::AssetCatalog *new_catalog = catalog_service->create_catalog(fullpath);
  if (!new_catalog) {
    return nullptr;
  }
  catalog_service->tag_has_unsaved_changes(new_catalog);

  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  return new_catalog;
}

void ED_asset_catalog_remove(::AssetLibrary *library, const CatalogID &catalog_id)
{
  asset_system::AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(
      library);
  if (!catalog_service) {
    BLI_assert_unreachable();
    return;
  }
  if (ED_asset_catalogs_read_only(*library)) {
    return;
  }

  catalog_service->undo_push();
  catalog_service->tag_has_unsaved_changes(nullptr);
  catalog_service->prune_catalogs_by_id(catalog_id);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

void ED_asset_catalog_rename(::AssetLibrary *library,
                             const CatalogID catalog_id,
                             const StringRefNull new_name)
{
  asset_system::AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(
      library);
  if (!catalog_service) {
    BLI_assert_unreachable();
    return;
  }
  if (ED_asset_catalogs_read_only(*library)) {
    return;
  }

  AssetCatalog *catalog = catalog_service->find_catalog(catalog_id);

  const AssetCatalogPath new_path = catalog->path.parent() / StringRef(new_name);
  const AssetCatalogPath clean_new_path = new_path.cleanup();

  if (new_path == catalog->path || clean_new_path == catalog->path) {
    /* Nothing changed, so don't bother renaming for nothing. */
    return;
  }

  catalog_service->undo_push();
  catalog_service->tag_has_unsaved_changes(catalog);
  catalog_service->update_catalog_path(catalog_id, clean_new_path);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

void ED_asset_catalog_move(::AssetLibrary *library,
                           const CatalogID src_catalog_id,
                           const std::optional<CatalogID> dst_parent_catalog_id)
{
  asset_system::AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(
      library);
  if (!catalog_service) {
    BLI_assert_unreachable();
    return;
  }
  if (ED_asset_catalogs_read_only(*library)) {
    return;
  }

  AssetCatalog *src_catalog = catalog_service->find_catalog(src_catalog_id);
  if (!src_catalog) {
    BLI_assert_unreachable();
    return;
  }
  AssetCatalog *dst_catalog = dst_parent_catalog_id ?
                                  catalog_service->find_catalog(*dst_parent_catalog_id) :
                                  nullptr;
  if (!dst_catalog && dst_parent_catalog_id) {
    BLI_assert_unreachable();
    return;
  }

  std::string unique_name = catalog_name_ensure_unique(
      *catalog_service, src_catalog->path.name(), dst_catalog ? dst_catalog->path.c_str() : "");
  /* If a destination catalog was given, construct the path using that. Otherwise, the path is just
   * the name of the catalog to be moved, which means it ends up at the root level. */
  const AssetCatalogPath new_path = dst_catalog ? (dst_catalog->path / unique_name) :
                                                  AssetCatalogPath{unique_name};
  const AssetCatalogPath clean_new_path = new_path.cleanup();

  if (new_path == src_catalog->path || clean_new_path == src_catalog->path) {
    /* Nothing changed, so don't bother renaming for nothing. */
    return;
  }

  catalog_service->undo_push();
  catalog_service->tag_has_unsaved_changes(src_catalog);
  catalog_service->update_catalog_path(src_catalog_id, clean_new_path);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

void ED_asset_catalogs_save_from_main_path(::AssetLibrary *library, const Main *bmain)
{
  asset_system::AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(
      library);
  if (!catalog_service) {
    BLI_assert_unreachable();
    return;
  }
  if (ED_asset_catalogs_read_only(*library)) {
    return;
  }

  /* Since writing to disk also means loading any on-disk changes, it may be a good idea to store
   * an undo step. */
  catalog_service->undo_push();
  catalog_service->write_to_disk(bmain->filepath);
}

void ED_asset_catalogs_set_save_catalogs_when_file_is_saved(const bool should_save)
{
  asset_system::AssetLibrary::save_catalogs_when_file_is_saved = should_save;
}

bool ED_asset_catalogs_get_save_catalogs_when_file_is_saved()
{
  return asset_system::AssetLibrary::save_catalogs_when_file_is_saved;
}

namespace blender::ed::asset {

PointerRNA persistent_catalog_path_rna_pointer(const bScreen &owner_screen,
                                               const asset_system::AssetLibrary &library,
                                               const asset_system::AssetCatalogTreeItem &item)
{
  const asset_system::AssetCatalog *catalog = library.catalog_service->find_catalog_by_path(
      item.catalog_path());
  if (!catalog) {
    return PointerRNA_NULL;
  }

  const asset_system::AssetCatalogPath &path = catalog->path;
  return {&const_cast<ID &>(owner_screen.id),
          &RNA_AssetCatalogPath,
          const_cast<asset_system::AssetCatalogPath *>(&path)};
}

}  // namespace blender::ed::asset
