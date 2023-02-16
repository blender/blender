/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <memory>

#include "AS_asset_catalog_tree.hh"
#include "AS_asset_identifier.hh"
#include "AS_asset_library.h"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_main.h"
#include "BKE_preferences.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "DNA_userdef_types.h"

#include "asset_library_service.hh"
#include "asset_storage.hh"
#include "utils.hh"

using namespace blender;
using namespace blender::asset_system;

bool asset_system::AssetLibrary::save_catalogs_when_file_is_saved = true;

/* Can probably removed once #WITH_DESTROY_VIA_LOAD_HANDLER gets enabled by default. */
void AS_asset_libraries_exit()
{
  AssetLibraryService::destroy();
}

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

std::string AS_asset_library_root_path_from_library_ref(
    const AssetLibraryReference &library_reference)
{
  return AssetLibraryService::root_path_from_library_ref(library_reference);
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
      [mappings](asset_system::AssetLibrary &library) {
        library.remap_ids_and_remove_invalid(*mappings);
      },
      true);
}

namespace blender::asset_system {

AssetLibrary::AssetLibrary(StringRef root_path)
    : root_path_(std::make_shared<std::string>(utils::normalize_directory_path(root_path))),
      asset_storage_(std::make_unique<AssetStorage>()),
      catalog_service(std::make_unique<AssetCatalogService>())
{
}

AssetLibrary::~AssetLibrary()
{
  if (on_save_callback_store_.func) {
    on_blend_save_handler_unregister();
  }
}

void AssetLibrary::foreach_loaded(FunctionRef<void(AssetLibrary &)> fn,
                                  const bool include_all_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  service->foreach_loaded_asset_library(fn, include_all_library);
}

void AssetLibrary::load_catalogs()
{
  auto catalog_service = std::make_unique<AssetCatalogService>(root_path());
  catalog_service->load_from_disk();
  this->catalog_service = std::move(catalog_service);
}

void AssetLibrary::refresh()
{
  if (on_refresh_) {
    on_refresh_(*this);
  }
}

AssetRepresentation &AssetLibrary::add_external_asset(StringRef relative_asset_path,
                                                      StringRef name,
                                                      std::unique_ptr<AssetMetaData> metadata)
{
  AssetIdentifier identifier = asset_identifier_from_library(relative_asset_path);
  return asset_storage_->add_external_asset(
      std::move(identifier), name, std::move(metadata), *this);
}

AssetRepresentation &AssetLibrary::add_local_id_asset(StringRef relative_asset_path, ID &id)
{
  AssetIdentifier identifier = asset_identifier_from_library(relative_asset_path);
  return asset_storage_->add_local_id_asset(std::move(identifier), id, *this);
}

bool AssetLibrary::remove_asset(AssetRepresentation &asset)
{
  return asset_storage_->remove_asset(asset);
}

void AssetLibrary::remap_ids_and_remove_invalid(const IDRemapper &mappings)
{
  asset_storage_->remap_ids_and_remove_invalid(mappings);
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

AssetIdentifier AssetLibrary::asset_identifier_from_library(StringRef relative_asset_path)
{
  return AssetIdentifier(root_path_, relative_asset_path);
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

StringRefNull AssetLibrary::root_path() const
{
  return *root_path_;
}

Vector<AssetLibraryReference> all_valid_asset_library_refs()
{
  Vector<AssetLibraryReference> result;
  {
    AssetLibraryReference library_ref{};
    library_ref.custom_library_index = -1;
    library_ref.type = ASSET_LIBRARY_ESSENTIALS;
    result.append(library_ref);
  }
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
