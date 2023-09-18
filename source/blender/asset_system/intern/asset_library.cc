/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_userdef_types.h"

#include "asset_library_service.hh"
#include "asset_storage.hh"
#include "utils.hh"

using namespace blender;
using namespace blender::asset_system;

bool asset_system::AssetLibrary::save_catalogs_when_file_is_saved = true;

void AS_asset_libraries_exit()
{
  /* NOTE: Can probably removed once #WITH_DESTROY_VIA_LOAD_HANDLER gets enabled by default. */

  AssetLibraryService::destroy();
}

asset_system::AssetLibrary *AS_asset_library_load(const Main *bmain,
                                                  const AssetLibraryReference &library_reference)
{
  AssetLibraryService *service = AssetLibraryService::get();
  return service->get_asset_library(bmain, library_reference);
}

::AssetLibrary *AS_asset_library_load(const char *name, const char *library_dirpath)
{
  /* NOTE: Loading an asset library at this point only means loading the catalogs.
   * Later on this should invoke reading of asset representations too. */

  AssetLibraryService *service = AssetLibraryService::get();
  asset_system::AssetLibrary *lib;
  if (library_dirpath == nullptr || library_dirpath[0] == '\0') {
    lib = service->get_asset_library_current_file();
  }
  else {
    lib = service->get_asset_library_on_disk_custom(name, library_dirpath);
  }
  return reinterpret_cast<::AssetLibrary *>(lib);
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
          &U, input_path.c_str()))
  {
    return preferences_lib->dirpath;
  }

  char buffer[FILE_MAXDIR];
  BLI_path_split_dir_part(input_path.c_str(), buffer, FILE_MAXDIR);
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

void AS_asset_library_refresh_catalog_simplename(::AssetLibrary *asset_library,
                                                 AssetMetaData *asset_data)
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

void AS_asset_full_path_explode_from_weak_ref(const AssetWeakReference *asset_reference,
                                              char r_path_buffer[1090 /* FILE_MAX_LIBEXTRA */],
                                              char **r_dir,
                                              char **r_group,
                                              char **r_name)
{
  AssetLibraryService *service = AssetLibraryService::get();
  std::optional<AssetLibraryService::ExplodedPath> exploded =
      service->resolve_asset_weak_reference_to_exploded_path(*asset_reference);

  if (!exploded) {
    if (r_dir) {
      *r_dir = nullptr;
    }
    if (r_group) {
      *r_group = nullptr;
    }
    if (r_name) {
      *r_name = nullptr;
    }
    r_path_buffer[0] = '\0';
    return;
  }

  BLI_assert(!exploded->group_component.is_empty());
  BLI_assert(!exploded->name_component.is_empty());

  BLI_strncpy(r_path_buffer, exploded->full_path->c_str(), 1090 /* FILE_MAX_LIBEXTRA */);

  if (!exploded->dir_component.is_empty()) {
    r_path_buffer[exploded->dir_component.size()] = '\0';
    r_path_buffer[exploded->dir_component.size() + 1 + exploded->group_component.size()] = '\0';

    if (r_dir) {
      *r_dir = r_path_buffer;
    }
    if (r_group) {
      *r_group = r_path_buffer + exploded->dir_component.size() + 1;
    }
    if (r_name) {
      *r_name = r_path_buffer + exploded->dir_component.size() + 1 +
                exploded->group_component.size() + 1;
    }
  }
  else {
    r_path_buffer[exploded->group_component.size()] = '\0';

    if (r_dir) {
      *r_dir = nullptr;
    }
    if (r_group) {
      *r_group = r_path_buffer;
    }
    if (r_name) {
      *r_name = r_path_buffer + exploded->group_component.size() + 1;
    }
  }
}

namespace blender::asset_system {

AssetLibrary::AssetLibrary(eAssetLibraryType library_type, StringRef name, StringRef root_path)
    : library_type_(library_type),
      name_(name),
      root_path_(std::make_shared<std::string>(utils::normalize_directory_path(root_path))),
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
                                                      const int id_type,
                                                      std::unique_ptr<AssetMetaData> metadata)
{
  AssetIdentifier identifier = asset_identifier_from_library(relative_asset_path);
  return asset_storage_->add_external_asset(
      std::move(identifier), name, id_type, std::move(metadata), *this);
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
void asset_library_on_save_post(Main *main,
                                PointerRNA **pointers,
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

void AssetLibrary::on_blend_save_post(Main *main,
                                      PointerRNA ** /*pointers*/,
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

std::string AssetLibrary::resolve_asset_weak_reference_to_full_path(
    const AssetWeakReference &asset_reference)
{
  AssetLibraryService *service = AssetLibraryService::get();
  return service->resolve_asset_weak_reference_to_full_path(asset_reference);
}

void AssetLibrary::refresh_catalog_simplename(AssetMetaData *asset_data)
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

eAssetLibraryType AssetLibrary::library_type() const
{
  return library_type_;
}

StringRefNull AssetLibrary::name() const
{
  return name_;
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
    if (!BLI_is_dir(asset_library->dirpath)) {
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

AssetLibraryReference all_library_reference()
{
  AssetLibraryReference all_library_ref{};
  all_library_ref.custom_library_index = -1;
  all_library_ref.type = ASSET_LIBRARY_ALL;
  return all_library_ref;
}

}  // namespace blender::asset_system
