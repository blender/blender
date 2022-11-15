/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <memory>

#include "AS_asset_library.h"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_preferences.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_set.hh"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "asset_library_service.hh"

using namespace blender;
using namespace blender::asset_system;

bool asset_system::AssetLibrary::save_catalogs_when_file_is_saved = true;

asset_system::AssetLibrary *AS_asset_library_load(const Main *bmain,
                                                  const AssetLibraryReference &library_reference)
{
  AssetLibraryService *service = AssetLibraryService::get();
  return service->get_asset_library(bmain, library_reference);
}

/**
 * Loading an asset library at this point only means loading the catalogs. Later on this should
 * invoke reading of asset representations too.
 */
struct ::AssetLibrary *AS_asset_library_load(const char *library_path)
{
  AssetLibraryService *service = AssetLibraryService::get();
  asset_system::AssetLibrary *lib;
  if (library_path == nullptr || library_path[0] == '\0') {
    lib = service->get_asset_library_current_file();
  }
  else {
    lib = service->get_asset_library_on_disk(library_path);
  }
  return reinterpret_cast<struct ::AssetLibrary *>(lib);
}

bool AS_asset_library_has_any_unsaved_catalogs()
{
  AssetLibraryService *service = AssetLibraryService::get();
  return service->has_any_unsaved_catalogs();
}

std::string AS_asset_library_find_suitable_root_path_from_path(
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

std::string AS_asset_library_find_suitable_root_path_from_main(const Main *bmain)
{
  return AS_asset_library_find_suitable_root_path_from_path(bmain->filepath);
}

AssetCatalogService *AS_asset_library_get_catalog_service(const ::AssetLibrary *library_c)
{
  if (library_c == nullptr) {
    return nullptr;
  }

  const asset_system::AssetLibrary &library = reinterpret_cast<const asset_system::AssetLibrary &>(
      *library_c);
  return library.catalog_service.get();
}

AssetCatalogTree *AS_asset_library_get_catalog_tree(const ::AssetLibrary *library)
{
  AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(library);
  if (catalog_service == nullptr) {
    return nullptr;
  }

  return catalog_service->get_catalog_tree();
}

void AS_asset_library_refresh_catalog_simplename(struct ::AssetLibrary *asset_library,
                                                 struct AssetMetaData *asset_data)
{
  asset_system::AssetLibrary *lib = reinterpret_cast<asset_system::AssetLibrary *>(asset_library);
  lib->refresh_catalog_simplename(asset_data);
}

void AS_asset_library_remap_ids(const IDRemapper *mappings)
{
  AssetLibraryService *service = AssetLibraryService::get();
  service->foreach_loaded_asset_library(
      [mappings](asset_system::AssetLibrary &library) { library.remap_ids(*mappings); });
}

namespace blender::asset_system {

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
  return *asset_storage_.lookup_key_or_add(
      std::make_unique<AssetRepresentation>(name, std::move(metadata)));
}

AssetRepresentation &AssetLibrary::add_local_id_asset(ID &id)
{
  return *asset_storage_.lookup_key_or_add(std::make_unique<AssetRepresentation>(id));
}

bool AssetLibrary::remove_asset(AssetRepresentation &asset)
{
  /* Create a "fake" unique_ptr to figure out the hash for the pointed to asset representation. The
   * standard requires that this is the same for all unique_ptr's wrapping the same address. */
  std::unique_ptr<AssetRepresentation> fake_asset_ptr{&asset};

  const bool was_removed = asset_storage_.remove_as(fake_asset_ptr);
  /* Make sure the contained storage is not destructed. */
  fake_asset_ptr.release();
  return was_removed;
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

void AssetLibrary::remap_ids(const IDRemapper &mappings)
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

}  // namespace blender::asset_system
