/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BKE_blender.h"
#include "BKE_preferences.h"

#include "BLI_string_ref.hh"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "CLG_log.h"

#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_essentials_library.hh"
#include "asset_library_service.hh"
#include "utils.hh"

/* When enabled, use a pre file load handler (#BKE_CB_EVT_LOAD_PRE) callback to destroy the asset
 * library service. Without this an explicit call from the file loading code is needed to do this,
 * which is not as nice.
 *
 * TODO Currently disabled because UI data depends on asset library data, so we have to make sure
 * it's freed in the right order (UI first). Pre-load handlers don't give us this order.
 * Should be addressed with a proper ownership model for the asset system:
 * https://wiki.blender.org/wiki/Source/Architecture/Asset_System/Back_End#Ownership_Model
 */
//#define WITH_DESTROY_VIA_LOAD_HANDLER

static CLG_LogRef LOG = {"asset_system.asset_library_service"};

namespace blender::asset_system {

std::unique_ptr<AssetLibraryService> AssetLibraryService::instance_;
bool AssetLibraryService::atexit_handler_registered_ = false;

AssetLibraryService *AssetLibraryService::get()
{
  if (!instance_) {
    allocate_service_instance();
  }
  return instance_.get();
}

void AssetLibraryService::destroy()
{
  if (!instance_) {
    return;
  }
  instance_->app_handler_unregister();
  instance_.reset();
}

AssetLibrary *AssetLibraryService::get_asset_library(
    const Main *bmain, const AssetLibraryReference &library_reference)
{
  const eAssetLibraryType type = eAssetLibraryType(library_reference.type);

  switch (type) {
    case ASSET_LIBRARY_ESSENTIALS: {
      const StringRefNull root_path = essentials_directory_path();

      AssetLibrary *library = get_asset_library_on_disk(root_path);
      library->import_method_ = ASSET_IMPORT_APPEND_REUSE;

      return library;
    }
    case ASSET_LIBRARY_LOCAL: {
      /* For the "Current File" library  we get the asset library root path based on main. */
      std::string root_path = bmain ? AS_asset_library_find_suitable_root_path_from_main(bmain) :
                                      "";

      if (root_path.empty()) {
        /* File wasn't saved yet. */
        return get_asset_library_current_file();
      }
      return get_asset_library_on_disk(root_path);
    }
    case ASSET_LIBRARY_ALL:
      return get_asset_library_all(bmain);
    case ASSET_LIBRARY_CUSTOM: {
      bUserAssetLibrary *custom_library = find_custom_asset_library_from_library_ref(
          library_reference);
      if (!custom_library) {
        return nullptr;
      }

      std::string root_path = custom_library->path;
      if (root_path.empty()) {
        return nullptr;
      }

      AssetLibrary *library = get_asset_library_on_disk(root_path);
      library->import_method_ = eAssetImportMethod(custom_library->import_method);
      library->may_override_import_method_ = true;

      return library;
    }
  }

  return nullptr;
}

AssetLibrary *AssetLibraryService::get_asset_library_on_disk(StringRefNull root_path)
{
  BLI_assert_msg(!root_path.is_empty(),
                 "top level directory must be given for on-disk asset library");

  std::string normalized_root_path = utils::normalize_directory_path(root_path);

  std::unique_ptr<AssetLibrary> *lib_uptr_ptr = on_disk_libraries_.lookup_ptr(
      normalized_root_path);
  if (lib_uptr_ptr != nullptr) {
    CLOG_INFO(&LOG, 2, "get \"%s\" (cached)", normalized_root_path.c_str());
    AssetLibrary *lib = lib_uptr_ptr->get();
    lib->refresh();
    return lib;
  }

  std::unique_ptr lib_uptr = std::make_unique<AssetLibrary>(normalized_root_path);
  AssetLibrary *lib = lib_uptr.get();

  lib->on_blend_save_handler_register();
  lib->load_catalogs();
  /* Reload catalogs on refresh. */
  lib->on_refresh_ = [](AssetLibrary &self) { self.catalog_service->reload_catalogs(); };

  on_disk_libraries_.add_new(normalized_root_path, std::move(lib_uptr));
  CLOG_INFO(&LOG, 2, "get \"%s\" (loaded)", normalized_root_path.c_str());
  return lib;
}

AssetLibrary *AssetLibraryService::get_asset_library_current_file()
{
  if (current_file_library_) {
    CLOG_INFO(&LOG, 2, "get current file lib (cached)");
    current_file_library_->refresh();
  }
  else {
    CLOG_INFO(&LOG, 2, "get current file lib (loaded)");
    current_file_library_ = std::make_unique<AssetLibrary>();
    current_file_library_->on_blend_save_handler_register();
  }

  AssetLibrary *lib = current_file_library_.get();
  return lib;
}

static void rebuild_all_library(AssetLibrary &all_library, const bool reload_catalogs)
{
  /* Start with empty catalog storage. */
  all_library.catalog_service = std::make_unique<AssetCatalogService>(
      AssetCatalogService::read_only_tag());

  AssetLibrary::foreach_loaded(
      [&](AssetLibrary &nested) {
        if (reload_catalogs) {
          nested.catalog_service->reload_catalogs();
        }
        all_library.catalog_service->add_from_existing(*nested.catalog_service);
      },
      false);
  all_library.catalog_service->rebuild_tree();
}

AssetLibrary *AssetLibraryService::get_asset_library_all(const Main *bmain)
{
  /* (Re-)load all other asset libraries. */
  for (AssetLibraryReference &library_ref : all_valid_asset_library_refs()) {
    /* Skip self :) */
    if (library_ref.type == ASSET_LIBRARY_ALL) {
      continue;
    }

    /* Ensure all asset libraries are loaded. */
    get_asset_library(bmain, library_ref);
  }

  if (all_library_) {
    CLOG_INFO(&LOG, 2, "get all lib (cached)");
    all_library_->refresh();
    return all_library_.get();
  }

  CLOG_INFO(&LOG, 2, "get all lib (loaded)");
  all_library_ = std::make_unique<AssetLibrary>();

  /* Don't reload catalogs on this initial read, they've just been loaded above. */
  rebuild_all_library(*all_library_, /*reload_catlogs=*/false);

  all_library_->on_refresh_ = [](AssetLibrary &all_library) {
    rebuild_all_library(all_library, /*reload_catalogs=*/true);
  };

  return all_library_.get();
}

bUserAssetLibrary *AssetLibraryService::find_custom_asset_library_from_library_ref(
    const AssetLibraryReference &library_reference)
{
  BLI_assert(library_reference.type == ASSET_LIBRARY_CUSTOM);
  BLI_assert(library_reference.custom_library_index >= 0);

  return BKE_preferences_asset_library_find_from_index(&U, library_reference.custom_library_index);
}

std::string AssetLibraryService::root_path_from_library_ref(
    const AssetLibraryReference &library_reference)
{
  if (ELEM(library_reference.type, ASSET_LIBRARY_ALL, ASSET_LIBRARY_LOCAL)) {
    return "";
  }
  if (ELEM(library_reference.type, ASSET_LIBRARY_ESSENTIALS)) {
    return essentials_directory_path();
  }

  bUserAssetLibrary *custom_library = find_custom_asset_library_from_library_ref(
      library_reference);
  if (!custom_library || !custom_library->path[0]) {
    return "";
  }

  return custom_library->path;
}

void AssetLibraryService::allocate_service_instance()
{
  instance_ = std::make_unique<AssetLibraryService>();
  instance_->app_handler_register();

  if (!atexit_handler_registered_) {
    /* Ensure the instance gets freed before Blender's memory leak detector runs. */
    BKE_blender_atexit_register([](void * /*user_data*/) { AssetLibraryService::destroy(); },
                                nullptr);
    atexit_handler_registered_ = true;
  }
}

static void on_blendfile_load(struct Main * /*bMain*/,
                              struct PointerRNA ** /*pointers*/,
                              const int /*num_pointers*/,
                              void * /*arg*/)
{
#ifdef WITH_DESTROY_VIA_LOAD_HANDLER
  AssetLibraryService::destroy();
#endif
}

void AssetLibraryService::app_handler_register()
{
  /* The callback system doesn't own `on_load_callback_store_`. */
  on_load_callback_store_.alloc = false;

  on_load_callback_store_.func = &on_blendfile_load;
  on_load_callback_store_.arg = this;

  BKE_callback_add(&on_load_callback_store_, BKE_CB_EVT_LOAD_PRE);
}

void AssetLibraryService::app_handler_unregister()
{
  BKE_callback_remove(&on_load_callback_store_, BKE_CB_EVT_LOAD_PRE);
  on_load_callback_store_.func = nullptr;
  on_load_callback_store_.arg = nullptr;
}

bool AssetLibraryService::has_any_unsaved_catalogs() const
{
  bool has_unsaved_changes = false;

  foreach_loaded_asset_library(
      [&has_unsaved_changes](AssetLibrary &library) {
        if (library.catalog_service->has_unsaved_changes()) {
          has_unsaved_changes = true;
        }
      },
      true);
  return has_unsaved_changes;
}

void AssetLibraryService::foreach_loaded_asset_library(FunctionRef<void(AssetLibrary &)> fn,
                                                       const bool include_all_library) const
{
  if (include_all_library && all_library_) {
    fn(*all_library_);
  }

  if (current_file_library_) {
    fn(*current_file_library_);
  }

  for (const auto &asset_lib_uptr : on_disk_libraries_.values()) {
    fn(*asset_lib_uptr);
  }
}

}  // namespace blender::asset_system
