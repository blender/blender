/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <memory>

#include "BKE_asset_library.hh"
#include "BKE_asset_representation.hh"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_preferences.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_set.hh"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "asset_library_service.hh"

bool blender::bke::AssetLibrary::save_catalogs_when_file_is_saved = true;

blender::bke::AssetLibrary *BKE_asset_library_load(const Main *bmain,
                                                   const AssetLibraryReference &library_reference)
{
  blender::bke::AssetLibraryService *service = blender::bke::AssetLibraryService::get();
  return service->get_asset_library(bmain, library_reference);
}

/**
 * Loading an asset library at this point only means loading the catalogs. Later on this should
 * invoke reading of asset representations too.
 */
struct AssetLibrary *BKE_asset_library_load(const char *library_path)
{
  blender::bke::AssetLibraryService *service = blender::bke::AssetLibraryService::get();
  blender::bke::AssetLibrary *lib;
  if (library_path == nullptr || library_path[0] == '\0') {
    lib = service->get_asset_library_current_file();
  }
  else {
    lib = service->get_asset_library_on_disk(library_path);
  }
  return reinterpret_cast<struct AssetLibrary *>(lib);
}

bool BKE_asset_library_has_any_unsaved_catalogs()
{
  blender::bke::AssetLibraryService *service = blender::bke::AssetLibraryService::get();
  return service->has_any_unsaved_catalogs();
}

std::string BKE_asset_library_find_suitable_root_path_from_path(
    const blender::StringRefNull input_path)
{
  if (bUserAssetLibrary *preferences_lib = BKE_preferences_asset_library_containing_path(
          &U, input_path.c_str())) {
    return preferences_lib->path;
  }

  char buffer[FILE_MAXDIR];
  BLI_split_dir_part(input_path.c_str(), buffer, FILE_MAXDIR);
  return buffer;
}

std::string BKE_asset_library_find_suitable_root_path_from_main(const Main *bmain)
{
  return BKE_asset_library_find_suitable_root_path_from_path(bmain->filepath);
}

blender::bke::AssetCatalogService *BKE_asset_library_get_catalog_service(
    const ::AssetLibrary *library_c)
{
  if (library_c == nullptr) {
    return nullptr;
  }

  const blender::bke::AssetLibrary &library = reinterpret_cast<const blender::bke::AssetLibrary &>(
      *library_c);
  return library.catalog_service.get();
}

blender::bke::AssetCatalogTree *BKE_asset_library_get_catalog_tree(const ::AssetLibrary *library)
{
  blender::bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(
      library);
  if (catalog_service == nullptr) {
    return nullptr;
  }

  return catalog_service->get_catalog_tree();
}

void BKE_asset_library_refresh_catalog_simplename(struct AssetLibrary *asset_library,
                                                  struct AssetMetaData *asset_data)
{
  blender::bke::AssetLibrary *lib = reinterpret_cast<blender::bke::AssetLibrary *>(asset_library);
  lib->refresh_catalog_simplename(asset_data);
}

void BKE_asset_library_remap_ids(IDRemapper *mappings)
{
  blender::bke::AssetLibraryService *service = blender::bke::AssetLibraryService::get();
  service->foreach_loaded_asset_library(
      [mappings](blender::bke::AssetLibrary &library) { library.remap_ids(*mappings); });
}

namespace blender::bke {

AssetLibrary::AssetLibrary() : catalog_service(std::make_unique<AssetCatalogService>())
{
}

AssetLibrary::~AssetLibrary()
{
  if (on_save_callback_store_.func) {
    on_blend_save_handler_unregister();
  }
}

void AssetLibrary::load_catalogs(StringRefNull library_root_directory)
{
  auto catalog_service = std::make_unique<AssetCatalogService>(library_root_directory);
  catalog_service->load_from_disk();
  this->catalog_service = std::move(catalog_service);
}

void AssetLibrary::refresh()
{
  this->catalog_service->reload_catalogs();
}

AssetRepresentation &AssetLibrary::add_external_asset(StringRef name,
                                                      std::unique_ptr<AssetMetaData> metadata)
{
  asset_storage_.append(std::make_unique<AssetRepresentation>(name, std::move(metadata)));
  return *asset_storage_.last();
}

AssetRepresentation &AssetLibrary::add_local_id_asset(ID &id)
{
  asset_storage_.append(std::make_unique<AssetRepresentation>(id));
  return *asset_storage_.last();
}

std::optional<int> AssetLibrary::find_asset_index(const AssetRepresentation &asset)
{
  int index = 0;
  /* Find index of asset. */
  for (auto &asset_uptr : asset_storage_) {
    if (&asset == asset_uptr.get()) {
      return index;
    }
    index++;
  }

  return {};
}

bool AssetLibrary::remove_asset(AssetRepresentation &asset)
{
  std::optional<int> asset_index = find_asset_index(asset);
  if (!asset_index) {
    return false;
  }

  asset_storage_.remove_and_reorder(*asset_index);
  return true;
}

namespace {
void asset_library_on_save_post(struct Main *main,
                                struct PointerRNA **pointers,
                                const int num_pointers,
                                void *arg)
{
  AssetLibrary *asset_lib = static_cast<AssetLibrary *>(arg);
  asset_lib->on_blend_save_post(main, pointers, num_pointers);
}

}  // namespace

void AssetLibrary::on_blend_save_handler_register()
{
  /* The callback system doesn't own `on_save_callback_store_`. */
  on_save_callback_store_.alloc = false;

  on_save_callback_store_.func = asset_library_on_save_post;
  on_save_callback_store_.arg = this;

  BKE_callback_add(&on_save_callback_store_, BKE_CB_EVT_SAVE_POST);
}

void AssetLibrary::on_blend_save_handler_unregister()
{
  BKE_callback_remove(&on_save_callback_store_, BKE_CB_EVT_SAVE_POST);
  on_save_callback_store_.func = nullptr;
  on_save_callback_store_.arg = nullptr;
}

void AssetLibrary::on_blend_save_post(struct Main *main,
                                      struct PointerRNA ** /*pointers*/,
                                      const int /*num_pointers*/)
{
  if (this->catalog_service == nullptr) {
    return;
  }

  if (save_catalogs_when_file_is_saved) {
    this->catalog_service->write_to_disk(main->filepath);
  }
}

void AssetLibrary::remap_ids(IDRemapper &mappings)
{
  Set<AssetRepresentation *> removed_id_assets;

  for (auto &asset_uptr : asset_storage_) {
    if (!asset_uptr->is_local_id()) {
      continue;
    }

    IDRemapperApplyResult result = BKE_id_remapper_apply(
        &mappings, &asset_uptr->local_asset_id_, ID_REMAP_APPLY_DEFAULT);
    if (result == ID_REMAP_RESULT_SOURCE_UNASSIGNED) {
      removed_id_assets.add(asset_uptr.get());
    }
  }

  /* Remove the assets from storage. */
  for (AssetRepresentation *asset : removed_id_assets) {
    remove_asset(*asset);
  }
}

void AssetLibrary::refresh_catalog_simplename(struct AssetMetaData *asset_data)
{
  if (BLI_uuid_is_nil(asset_data->catalog_id)) {
    asset_data->catalog_simple_name[0] = '\0';
    return;
  }
  const AssetCatalog *catalog = this->catalog_service->find_catalog(asset_data->catalog_id);
  if (catalog == nullptr) {
    /* No-op if the catalog cannot be found. This could be the kind of "the catalog definition file
     * is corrupt/lost" scenario that the simple name is meant to help recover from. */
    return;
  }
  STRNCPY(asset_data->catalog_simple_name, catalog->simple_name.c_str());
}

Vector<AssetLibraryReference> all_valid_asset_library_refs()
{
  Vector<AssetLibraryReference> result;
  int i;
  LISTBASE_FOREACH_INDEX (const bUserAssetLibrary *, asset_library, &U.asset_libraries, i) {
    if (!BLI_is_dir(asset_library->path)) {
      continue;
    }
    AssetLibraryReference library_ref{};
    library_ref.custom_library_index = i;
    library_ref.type = ASSET_LIBRARY_CUSTOM;
    result.append(library_ref);
  }

  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = -1;
  library_ref.type = ASSET_LIBRARY_LOCAL;
  result.append(library_ref);
  return result;
}

}  // namespace blender::bke
