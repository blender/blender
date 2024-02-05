/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BKE_blender.h"
#include "BKE_preferences.h"

#include "BLI_path_util.h"
#include "BLI_string_ref.hh"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "CLG_log.h"

#include "AS_asset_library.hh"
#include "AS_essentials_library.hh"
#include "asset_library_all.hh"
#include "asset_library_on_disk.hh"
#include "asset_library_runtime.hh"
#include "asset_library_service.hh"
#include "utils.hh"

/* When enabled, use a pre file load handler (#BKE_CB_EVT_LOAD_PRE) callback to destroy the asset
 * library service. Without this an explicit call from the file loading code is needed to do this,
 * which is not as nice.
 *
 * TODO Currently disabled because UI data depends on asset library data, so we have to make sure
 * it's freed in the right order (UI first). Pre-load handlers don't give us this order.
 * Should be addressed with a proper ownership model for the asset system:
 * https://developer.blender.org/docs/features/asset_system/backend/#ownership-model
 */
// #define WITH_DESTROY_VIA_LOAD_HANDLER

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
      if (root_path.is_empty()) {
        return nullptr;
      }

      AssetLibrary *library = get_asset_library_on_disk_builtin(type, root_path);
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
      return get_asset_library_on_disk_builtin(type, root_path);
    }
    case ASSET_LIBRARY_ALL:
      return get_asset_library_all(bmain);
    case ASSET_LIBRARY_CUSTOM: {
      bUserAssetLibrary *custom_library = find_custom_asset_library_from_library_ref(
          library_reference);
      if (!custom_library) {
        return nullptr;
      }

      std::string root_path = custom_library->dirpath;
      if (root_path.empty()) {
        return nullptr;
      }

      AssetLibrary *library = get_asset_library_on_disk_custom(custom_library->name, root_path);
      library->import_method_ = eAssetImportMethod(custom_library->import_method);
      library->may_override_import_method_ = true;
      library->use_relative_path_ = (custom_library->flag & ASSET_LIBRARY_RELATIVE_PATH) != 0;

      return library;
    }
  }

  return nullptr;
}

AssetLibrary *AssetLibraryService::get_asset_library_on_disk(eAssetLibraryType library_type,
                                                             StringRef name,
                                                             StringRefNull root_path)
{
  BLI_assert_msg(!root_path.is_empty(),
                 "top level directory must be given for on-disk asset library");

  std::string normalized_root_path = utils::normalize_directory_path(root_path);

  std::unique_ptr<OnDiskAssetLibrary> *lib_uptr_ptr = on_disk_libraries_.lookup_ptr(
      {library_type, normalized_root_path});
  if (lib_uptr_ptr != nullptr) {
    CLOG_INFO(&LOG, 2, "get \"%s\" (cached)", normalized_root_path.c_str());
    AssetLibrary *lib = lib_uptr_ptr->get();
    lib->refresh_catalogs();
    return lib;
  }

  std::unique_ptr lib_uptr = std::make_unique<OnDiskAssetLibrary>(
      library_type, name, normalized_root_path);
  AssetLibrary *lib = lib_uptr.get();

  lib->load_catalogs();

  on_disk_libraries_.add_new({library_type, normalized_root_path}, std::move(lib_uptr));
  CLOG_INFO(&LOG, 2, "get \"%s\" (loaded)", normalized_root_path.c_str());
  return lib;
}

AssetLibrary *AssetLibraryService::get_asset_library_on_disk_custom(StringRef name,
                                                                    StringRefNull root_path)
{
  return get_asset_library_on_disk(ASSET_LIBRARY_CUSTOM, name, root_path);
}

AssetLibrary *AssetLibraryService::get_asset_library_on_disk_builtin(eAssetLibraryType type,
                                                                     StringRefNull root_path)
{
  BLI_assert_msg(
      type != ASSET_LIBRARY_CUSTOM,
      "Use `get_asset_library_on_disk_custom()` for libraries of type `ASSET_LIBRARY_CUSTOM`");

  /* Builtin asset libraries don't need a name, the #eAssetLibraryType is enough to identify them
   * (and doesn't change, unlike the name). */
  return get_asset_library_on_disk(type, {}, root_path);
}

AssetLibrary *AssetLibraryService::get_asset_library_current_file()
{
  if (current_file_library_) {
    CLOG_INFO(&LOG, 2, "get current file lib (cached)");
    current_file_library_->refresh_catalogs();
  }
  else {
    CLOG_INFO(&LOG, 2, "get current file lib (loaded)");
    current_file_library_ = std::make_unique<RuntimeAssetLibrary>();
  }

  AssetLibrary *lib = current_file_library_.get();
  return lib;
}

void AssetLibraryService::rebuild_all_library()
{
  if (all_library_) {
    all_library_->rebuild(false);
  }
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
    all_library_->refresh_catalogs();
    return all_library_.get();
  }

  CLOG_INFO(&LOG, 2, "get all lib (loaded)");
  all_library_ = std::make_unique<AllAssetLibrary>();

  /* Don't reload catalogs on this initial read, they've just been loaded above. */
  all_library_->rebuild(/*reload_catalogs=*/false);

  return all_library_.get();
}

bUserAssetLibrary *AssetLibraryService::find_custom_preferences_asset_library_from_asset_weak_ref(
    const AssetWeakReference &asset_reference)
{
  if (!ELEM(asset_reference.asset_library_type, ASSET_LIBRARY_CUSTOM)) {
    return nullptr;
  }

  return BKE_preferences_asset_library_find_by_name(&U, asset_reference.asset_library_identifier);
}

AssetLibrary *AssetLibraryService::find_loaded_on_disk_asset_library_from_name(
    StringRef name) const
{
  for (const std::unique_ptr<OnDiskAssetLibrary> &library : on_disk_libraries_.values()) {
    if (library->name_ == name) {
      return library.get();
    }
  }
  return nullptr;
}

std::string AssetLibraryService::resolve_asset_weak_reference_to_library_path(
    const AssetWeakReference &asset_reference)
{
  StringRefNull library_dirpath;

  switch (eAssetLibraryType(asset_reference.asset_library_type)) {
    case ASSET_LIBRARY_CUSTOM: {
      bUserAssetLibrary *custom_lib = find_custom_preferences_asset_library_from_asset_weak_ref(
          asset_reference);
      if (custom_lib) {
        library_dirpath = custom_lib->dirpath;
        break;
      }

      /* A bit of an odd-ball, the API supports loading custom libraries from arbitrary paths (used
       * by unit tests). So check all loaded on-disk libraries too. */
      AssetLibrary *loaded_custom_lib = find_loaded_on_disk_asset_library_from_name(
          asset_reference.asset_library_identifier);
      if (!loaded_custom_lib) {
        return "";
      }

      library_dirpath = *loaded_custom_lib->root_path_;
      break;
    }
    case ASSET_LIBRARY_ESSENTIALS:
      library_dirpath = essentials_directory_path();
      break;
    case ASSET_LIBRARY_LOCAL:
    case ASSET_LIBRARY_ALL:
      return "";
  }

  std::string normalized_library_dirpath = utils::normalize_path(library_dirpath);
  return normalized_library_dirpath;
}

int64_t AssetLibraryService::rfind_blendfile_extension(StringRef path)
{
  const std::vector<StringRefNull> blendfile_extensions = {".blend" SEP_STR,
                                                           ".blend.gz" SEP_STR,
                                                           ".ble" SEP_STR,
                                                           ".blend" ALTSEP_STR,
                                                           ".blend.gz" ALTSEP_STR,
                                                           ".ble" ALTSEP_STR};
  int64_t blendfile_extension_pos = StringRef::not_found;

  for (StringRefNull blendfile_ext : blendfile_extensions) {
    const int64_t iter_ext_pos = path.rfind(blendfile_ext);
    if (iter_ext_pos == StringRef::not_found) {
      continue;
    }

    if ((blendfile_extension_pos == StringRef::not_found) ||
        (blendfile_extension_pos < iter_ext_pos))
    {
      blendfile_extension_pos = iter_ext_pos;
    }
  }

  return blendfile_extension_pos;
}

std::string AssetLibraryService::normalize_asset_weak_reference_relative_asset_identifier(
    const AssetWeakReference &asset_reference)
{
  StringRefNull relative_asset_identifier = asset_reference.relative_asset_identifier;

  int64_t blend_ext_pos = rfind_blendfile_extension(asset_reference.relative_asset_identifier);
  const bool has_blend_ext = blend_ext_pos != StringRef::not_found;

  int64_t blend_path_len = 0;
  /* Get the position of the path separator after the blend file extension. */
  if (has_blend_ext) {
    blend_path_len = relative_asset_identifier.find_first_of(SEP_STR ALTSEP_STR, blend_ext_pos);

    /* If there is a blend file in the relative asset path, then there should be group and id name
     * after it. */
    BLI_assert(blend_path_len != StringRef::not_found);
    /* Skip slash. */
    blend_path_len += 1;
  }

  /* Find the first path separator (after the blend file extension if any). This will be the one
   * separating the group from the name. */
  const int64_t group_name_sep_pos = relative_asset_identifier.find_first_of(SEP_STR ALTSEP_STR,
                                                                             blend_path_len);

  return utils::normalize_path(relative_asset_identifier,
                               (group_name_sep_pos == StringRef::not_found) ?
                                   StringRef::not_found :
                                   group_name_sep_pos + 1);
}

/* TODO currently only works for asset libraries on disk (custom or essentials asset libraries).
 * Once there is a proper registry of asset libraries, this could contain an asset library locator
 * and/or identifier, so a full path (not necessarily file path) can be built for all asset
 * libraries. */
std::string AssetLibraryService::resolve_asset_weak_reference_to_full_path(
    const AssetWeakReference &asset_reference)
{
  if (asset_reference.relative_asset_identifier[0] == '\0') {
    return "";
  }

  std::string library_dirpath = resolve_asset_weak_reference_to_library_path(asset_reference);
  if (library_dirpath.empty()) {
    return "";
  }

  std::string normalized_full_path = utils::normalize_path(library_dirpath + SEP_STR) +
                                     normalize_asset_weak_reference_relative_asset_identifier(
                                         asset_reference);

  return normalized_full_path;
}

std::optional<AssetLibraryService::ExplodedPath> AssetLibraryService::
    resolve_asset_weak_reference_to_exploded_path(const AssetWeakReference &asset_reference)
{
  if (asset_reference.relative_asset_identifier[0] == '\0') {
    return std::nullopt;
  }

  switch (eAssetLibraryType(asset_reference.asset_library_type)) {
    case ASSET_LIBRARY_LOCAL: {
      std::string path_in_file = normalize_asset_weak_reference_relative_asset_identifier(
          asset_reference);
      const int64_t group_len = int64_t(path_in_file.find(SEP));

      ExplodedPath exploded;
      exploded.full_path = std::make_unique<std::string>(path_in_file);
      exploded.group_component = StringRef(*exploded.full_path).substr(0, group_len);
      exploded.name_component = StringRef(*exploded.full_path).substr(group_len + 1);

      return exploded;
    }
    case ASSET_LIBRARY_CUSTOM:
    case ASSET_LIBRARY_ESSENTIALS: {
      std::string full_path = resolve_asset_weak_reference_to_full_path(asset_reference);
      /* #full_path uses native slashes, so others don't need to be considered in the following. */

      if (full_path.empty()) {
        return std::nullopt;
      }

      int64_t blendfile_extension_pos = rfind_blendfile_extension(full_path);
      BLI_assert(blendfile_extension_pos != StringRef::not_found);

      size_t group_pos = full_path.find(SEP, blendfile_extension_pos);
      BLI_assert(group_pos != std::string::npos);

      size_t name_pos = full_path.find(SEP, group_pos + 1);
      BLI_assert(group_pos != std::string::npos);

      const int64_t dir_len = int64_t(group_pos);
      const int64_t group_len = int64_t(name_pos - group_pos - 1);

      ExplodedPath exploded;
      exploded.full_path = std::make_unique<std::string>(full_path);
      StringRef full_path_ref = *exploded.full_path;
      exploded.dir_component = full_path_ref.substr(0, dir_len);
      exploded.group_component = full_path_ref.substr(dir_len + 1, group_len);
      exploded.name_component = full_path_ref.substr(dir_len + 1 + group_len + 1);

      return exploded;
    }
    case ASSET_LIBRARY_ALL:
      return std::nullopt;
  }

  return std::nullopt;
}

bUserAssetLibrary *AssetLibraryService::find_custom_asset_library_from_library_ref(
    const AssetLibraryReference &library_reference)
{
  BLI_assert(library_reference.type == ASSET_LIBRARY_CUSTOM);
  BLI_assert(library_reference.custom_library_index >= 0);

  return BKE_preferences_asset_library_find_index(&U, library_reference.custom_library_index);
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
  if (!custom_library || !custom_library->dirpath[0]) {
    return "";
  }

  return custom_library->dirpath;
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

static void on_blendfile_load(Main * /*bMain*/,
                              PointerRNA ** /*pointers*/,
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
