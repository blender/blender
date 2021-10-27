/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edasset
 */

#include "BKE_asset_catalog.hh"
#include "BKE_asset_catalog_path.hh"
#include "BKE_asset_library.hh"
#include "BKE_main.h"

#include "BLI_string_utils.h"

#include "ED_asset_catalog.h"
#include "ED_asset_catalog.hh"

#include "WM_api.h"

using namespace blender;
using namespace blender::bke;

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

AssetCatalog *ED_asset_catalog_add(::AssetLibrary *library,
                                   StringRefNull name,
                                   StringRef parent_path)
{
  bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(library);
  if (!catalog_service) {
    return nullptr;
  }

  std::string unique_name = catalog_name_ensure_unique(*catalog_service, name, parent_path);
  AssetCatalogPath fullpath = AssetCatalogPath(parent_path) / unique_name;

  catalog_service->undo_push();
  bke::AssetCatalog *new_catalog = catalog_service->create_catalog(fullpath);
  if (!new_catalog) {
    return nullptr;
  }
  catalog_service->tag_has_unsaved_changes(new_catalog);

  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  return new_catalog;
}

void ED_asset_catalog_remove(::AssetLibrary *library, const CatalogID &catalog_id)
{
  bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(library);
  if (!catalog_service) {
    BLI_assert_unreachable();
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
  bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(library);
  if (!catalog_service) {
    BLI_assert_unreachable();
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
                           const CatalogID dst_parent_catalog_id)
{
  bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(library);
  if (!catalog_service) {
    BLI_assert_unreachable();
    return;
  }

  AssetCatalog *src_catalog = catalog_service->find_catalog(src_catalog_id);
  AssetCatalog *dst_catalog = catalog_service->find_catalog(dst_parent_catalog_id);

  const AssetCatalogPath new_path = dst_catalog->path / StringRef(src_catalog->path.name());
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
  bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(library);
  if (!catalog_service) {
    BLI_assert_unreachable();
    return;
  }

  /* Since writing to disk also means loading any on-disk changes, it may be a good idea to store
   * an undo step. */
  catalog_service->undo_push();
  catalog_service->write_to_disk(bmain->name);
}

void ED_asset_catalogs_set_save_catalogs_when_file_is_saved(const bool should_save)
{
  bke::AssetLibrary::save_catalogs_when_file_is_saved = should_save;
}

bool ED_asset_catalogs_get_save_catalogs_when_file_is_saved()
{
  return bke::AssetLibrary::save_catalogs_when_file_is_saved;
}
