/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "AS_asset_library.hh"

#include "BLI_function_ref.hh"
#include "BLI_map.hh"

#include <memory>

struct AssetLibraryReference;
struct bUserAssetLibrary;

namespace blender::asset_system {

/**
 * Global singleton-ish that provides access to individual #AssetLibrary instances.
 *
 * Whenever a blend file is loaded, the existing instance of AssetLibraryService is destructed, and
 * a new one is created -- hence the "singleton-ish". This ensures only information about relevant
 * asset libraries is loaded.
 *
 * \note How Asset libraries are identified may change in the future.
 *  For now they are assumed to be:
 * - on disk (identified by the absolute directory), or
 * - the "current file" library (which is in memory but could have catalogs
 *   loaded from a file on disk).
 */
class AssetLibraryService {
  static std::unique_ptr<AssetLibraryService> instance_;

  /* Mapping absolute path of the library's root path (normalize with #normalize_directory_path()!)
   * the AssetLibrary instance. */
  Map<std::string, std::unique_ptr<AssetLibrary>> on_disk_libraries_;
  /** Library without a known path, i.e. the "Current File" library if the file isn't saved yet. If
   * the file was saved, a valid path for the library can be determined and #on_disk_libraries_
   * above should be used. */
  std::unique_ptr<AssetLibrary> current_file_library_;
  /** The "all" asset library, merging all other libraries into one. */
  std::unique_ptr<AssetLibrary> all_library_;

  /* Handlers for managing the life cycle of the AssetLibraryService instance. */
  bCallbackFuncStore on_load_callback_store_;
  static bool atexit_handler_registered_;

 public:
  AssetLibraryService() = default;
  ~AssetLibraryService() = default;

  /** Return the AssetLibraryService singleton, allocating it if necessary. */
  static AssetLibraryService *get();

  /** Destroy the AssetLibraryService singleton. It will be reallocated by #get() if necessary. */
  static void destroy();

  static std::string root_path_from_library_ref(const AssetLibraryReference &library_reference);
  static bUserAssetLibrary *find_custom_asset_library_from_library_ref(
      const AssetLibraryReference &library_reference);

  AssetLibrary *get_asset_library(const Main *bmain,
                                  const AssetLibraryReference &library_reference);

  /**
   * Get the given asset library. Opens it (i.e. creates a new AssetLibrary instance) if necessary.
   */
  AssetLibrary *get_asset_library_on_disk(StringRefNull top_level_directory);

  /** Get the "Current File" asset library. */
  AssetLibrary *get_asset_library_current_file();

  /** Get the "All" asset library, which loads all others and merges them into one. */
  AssetLibrary *get_asset_library_all(const Main *bmain);

  /** Returns whether there are any known asset libraries with unsaved catalog edits. */
  bool has_any_unsaved_catalogs() const;

  /** See AssetLibrary::foreach_loaded(). */
  void foreach_loaded_asset_library(FunctionRef<void(AssetLibrary &)> fn,
                                    bool include_all_library) const;

 protected:
  /** Allocate a new instance of the service and assign it to `instance_`. */
  static void allocate_service_instance();

  /**
   * Ensure the AssetLibraryService instance is destroyed before a new blend file is loaded.
   * This makes memory management simple, and ensures a fresh start for every blend file. */
  void app_handler_register();
  void app_handler_unregister();
};

}  // namespace blender::asset_system
