/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <memory>

#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "asset_catalog_collection.hh"
#include "asset_catalog_definition_file.hh"
#include "asset_library_service.hh"
#include "essentials_library.hh"
#include "runtime_library.hh"
#include "utils.hh"

using namespace blender;
using namespace blender::asset_system;

bool AssetLibrary::save_catalogs_when_file_is_saved = true;

void AS_asset_libraries_exit()
{
  /* NOTE: Can probably removed once #WITH_DESTROY_VIA_LOAD_HANDLER gets enabled by default. */

  AssetLibraryService::destroy();
}

AssetLibrary *AS_asset_library_load(const Main *bmain,
                                    const AssetLibraryReference &library_reference)
{
  AssetLibraryService *service = AssetLibraryService::get();
  return service->get_asset_library(bmain, library_reference);
}

AssetLibrary *AS_asset_library_load_from_directory(const char *name, const char *library_dirpath)
{
  /* NOTE: Loading an asset library at this point only means loading the catalogs.
   * Later on this should invoke reading of asset representations too. */

  AssetLibraryService *service = AssetLibraryService::get();
  AssetLibrary *lib;
  if (library_dirpath == nullptr || library_dirpath[0] == '\0') {
    lib = service->get_asset_library_current_file();
  }
  else {
    lib = service->get_asset_library_on_disk_custom(name, library_dirpath);
  }
  return lib;
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

void AS_asset_library_remap_ids(const bke::id::IDRemapper &mappings)
{
  AssetLibraryService *service = AssetLibraryService::get();
  service->foreach_loaded_asset_library(
      [mappings](AssetLibrary &library) { library.remap_ids_and_remove_invalid(mappings); }, true);
}

void AS_asset_full_path_explode_from_weak_ref(const AssetWeakReference *asset_reference,
                                              char r_path_buffer[/*FILE_MAX_LIBEXTRA*/ 1282],
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

  BLI_strncpy(r_path_buffer, exploded->full_path->c_str(), /*FILE_MAX_LIBEXTRA*/ 1282);

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

static void update_import_method_for_user_libraries()
{
  LISTBASE_FOREACH (bUserAssetLibrary *, library, &U.asset_libraries) {
    if (U.experimental.no_data_block_packing) {
      if (library->import_method == ASSET_IMPORT_PACK) {
        library->import_method = ASSET_IMPORT_APPEND_REUSE;
      }
    }
    else {
      if (library->import_method == ASSET_IMPORT_APPEND_REUSE) {
        library->import_method = ASSET_IMPORT_PACK;
      }
    }
  }
}

static void update_import_method_for_asset_browsers(Main &bmain)
{
  LISTBASE_FOREACH (bScreen *, screen, &bmain.screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype != SPACE_FILE) {
          continue;
        }
        SpaceFile *sfile = reinterpret_cast<SpaceFile *>(sl);
        if (!sfile->asset_params) {
          continue;
        }
        if (U.experimental.no_data_block_packing) {
          if (sfile->asset_params->import_method == FILE_ASSET_IMPORT_PACK) {
            sfile->asset_params->import_method = FILE_ASSET_IMPORT_APPEND_REUSE;
          }
        }
        else {
          if (sfile->asset_params->import_method == FILE_ASSET_IMPORT_APPEND_REUSE) {
            sfile->asset_params->import_method = FILE_ASSET_IMPORT_PACK;
          }
        }
      }
    }
  }
}

void AS_asset_library_import_method_ensure_valid(Main &bmain)
{
  update_import_method_for_user_libraries();
  update_import_method_for_asset_browsers(bmain);
}

void AS_asset_library_essential_import_method_update()
{
  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = -1;
  library_ref.type = ASSET_LIBRARY_ESSENTIALS;
  EssentialsAssetLibrary *library = dynamic_cast<EssentialsAssetLibrary *>(
      AS_asset_library_load(nullptr, library_ref));
  if (library) {
    library->update_default_import_method();
  }
}

namespace blender::asset_system {

AssetLibrary::AssetLibrary(eAssetLibraryType library_type, StringRef name, StringRef root_path)
    : library_type_(library_type),
      name_(name),
      root_path_(std::make_shared<std::string>(utils::normalize_directory_path(root_path))),
      catalog_service_(std::make_unique<AssetCatalogService>(*root_path_))
{
}

AssetLibrary::~AssetLibrary()
{
  if (on_save_callback_store_.func) {
    this->on_blend_save_handler_unregister();
  }
}

void AssetLibrary::foreach_loaded(FunctionRef<void(AssetLibrary &)> fn,
                                  const bool include_all_library)
{
  AssetLibraryService *service = AssetLibraryService::get();
  service->foreach_loaded_asset_library(fn, include_all_library);
}

void AssetLibrary::load_or_reload_catalogs()
{
  {
    std::lock_guard lock{catalog_service_mutex_};
    /* Should never actually be the case, catalog service gets allocated with the asset library. */
    if (catalog_service_ == nullptr) {
      auto catalog_service = std::make_unique<AssetCatalogService>(root_path());
      catalog_service->load_from_disk();
      catalog_service_ = std::move(catalog_service);
      return;
    }
  }

  /* The catalog service was created before without being associated with a definition file. */
  if (catalog_service_->get_catalog_definition_file() == nullptr) {
    catalog_service_->load_from_disk();
  }
  else {
    this->refresh_catalogs();
  }
}

AssetCatalogService &AssetLibrary::catalog_service() const
{
  return *catalog_service_;
}

void AssetLibrary::refresh_catalogs() {}

std::weak_ptr<AssetRepresentation> AssetLibrary::add_external_asset(
    StringRef relative_asset_path,
    StringRef name,
    const int id_type,
    std::unique_ptr<AssetMetaData> metadata)
{
  return asset_storage_.external_assets.lookup_key_or_add(std::make_shared<AssetRepresentation>(
      relative_asset_path, name, id_type, std::move(metadata), *this));
}

std::weak_ptr<AssetRepresentation> AssetLibrary::add_local_id_asset(ID &id)
{
  return asset_storage_.local_id_assets.lookup_key_or_add(
      std::make_shared<AssetRepresentation>(id, *this));
}

bool AssetLibrary::remove_asset(AssetRepresentation &asset)
{
  /* Make sure this is forwarded to the library actually owning the asset if needed. For example
   * the "All Libraries" library doesn't own the assets itself. */
  if (&asset.owner_asset_library_ != this) {
    return asset.owner_asset_library_.remove_asset(asset);
  }

  BLI_assert(asset_storage_.local_id_assets.contains_as(&asset) ||
             asset_storage_.external_assets.contains_as(&asset));

  if (asset_storage_.local_id_assets.remove_as(&asset)) {
    return true;
  }
  return asset_storage_.external_assets.remove_as(&asset);
}

void AssetLibrary::remap_ids_and_remove_invalid(const bke::id::IDRemapper &mappings)
{
  Set<AssetRepresentation *> removed_assets;

  for (const auto &asset_ptr : asset_storage_.local_id_assets) {
    AssetRepresentation &asset = *asset_ptr;
    BLI_assert(asset.is_local_id());

    const IDRemapperApplyResult result = mappings.apply(&std::get<ID *>(asset.asset_),
                                                        ID_REMAP_APPLY_DEFAULT);

    /* Entirely remove assets whose ID is unset. We don't want assets with a null ID pointer. */
    if (result == ID_REMAP_RESULT_SOURCE_UNASSIGNED) {
      removed_assets.add(&asset);
    }
  }

  for (AssetRepresentation *asset : removed_assets) {
    this->remove_asset(*asset);
  }
}

namespace {
void asset_library_on_save_post(Main *bmain,
                                PointerRNA **pointers,
                                const int num_pointers,
                                void *arg)
{
  AssetLibrary *asset_lib = static_cast<AssetLibrary *>(arg);

  /* Transform 'runtime' current file library into 'on-disk' current file library. */
  if (asset_lib->library_type() == ASSET_LIBRARY_LOCAL && asset_lib->root_path().is_empty()) {
    BLI_assert(dynamic_cast<RuntimeAssetLibrary *>(asset_lib) != nullptr);

    if (AssetLibrary *on_disk_lib =
            AssetLibraryService::move_runtime_current_file_into_on_disk_library(*bmain))
    {
      /* Allow undoing to the state before merging in catalogs from disk. */
      on_disk_lib->catalog_service().undo_push();

      /* Force refresh to merge on-disk catalogs with the ones stolen from the runtime library. */
      asset_lib = AssetLibraryService::get()->get_asset_library_on_disk_builtin(
          ASSET_LIBRARY_LOCAL, on_disk_lib->root_path());
      BLI_assert(asset_lib == on_disk_lib);
    }
  }

  asset_lib->on_blend_save_post(bmain, pointers, num_pointers);
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

void AssetLibrary::on_blend_save_post(Main *bmain,
                                      PointerRNA ** /*pointers*/,
                                      const int /*num_pointers*/)
{
  if (save_catalogs_when_file_is_saved) {
    this->catalog_service().write_to_disk(bmain->filepath);
  }
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
  const AssetCatalog *catalog = this->catalog_service().find_catalog(asset_data->catalog_id);
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

AssetLibraryReference current_file_library_reference()
{
  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = -1;
  library_ref.type = ASSET_LIBRARY_LOCAL;
  return library_ref;
}

void all_library_reload_catalogs_if_dirty()
{
  AssetLibraryService *service = AssetLibraryService::get();
  service->reload_all_library_catalogs_if_dirty();
}

}  // namespace blender::asset_system
