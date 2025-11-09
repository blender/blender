/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include <memory>
#include <optional>

#include "AS_asset_catalog.hh"

#include "DNA_asset_types.h"

#include "BLI_mutex.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_callbacks.hh"

struct Main;

namespace blender::bke::id {
class IDRemapper;
}

namespace blender::asset_system {

class AssetRepresentation;

/**
 * AssetLibrary provides access to an asset library's data.
 *
 * The asset library contains catalogs and storage for asset representations. It could be extended
 * to also include asset indexes and more.
 */
class AssetLibrary {
  eAssetLibraryType library_type_;
  /**
   * The name this asset library will be displayed as in the UI. Will also be used as a weak way
   * to identify an asset library (e.g. by #AssetWeakReference).
   */
  std::string name_;
  /** If this is an asset library on disk, the top-level directory path. Normalized using
   * #normalize_directory_path(). Shared pointer so assets can safely point to it, and don't have
   * to hold a copy (which is the size of `std::string` + the allocated buffer, if no short string
   * optimization is used). With thousands of assets this might make a reasonable difference. */
  std::shared_ptr<std::string> root_path_;

  /**
   * AssetStorage for assets (better said their representations) that are considered to be part of
   * this library. Assets are not automatically loaded into this when loading an asset library.
   * Assets have to be loaded externally and added to this storage via #add_external_asset() or
   * #add_local_id_asset(). So this really is arbitrary storage as far as #AssetLibrary is
   * concerned (allowing the API user to manage partial library storage and partial loading, so
   * only relevant parts of a library are kept in memory).
   *
   * For now, multiple parts of Blender just keep adding their own assets to this storage. E.g.
   * multiple asset browsers might load multiple representations for the same asset into this.
   * Currently there is just no way to properly identify assets, or keep track of which assets are
   * already in memory and which not. Neither do we keep track of how many parts of Blender are
   * using an asset or an asset library, which is needed to know when assets can be freed.
   */
  struct AssetStorage {
    /* Uses shared pointers so the UI can acquire weak pointers. It can then ensure pointers are
     * not dangling before accessing. */

    Set<std::shared_ptr<AssetRepresentation>> external_assets;
    /* Store local ID assets separately for efficient lookups.
     * TODO(Julian): A [ID *, asset] or even [ID.session_uid, asset] map would be preferable for
     * faster lookups. Not possible until each asset is only represented once in the storage. */
    Set<std::shared_ptr<AssetRepresentation>> local_id_assets;
  };
  AssetStorage asset_storage_;

 protected:
  /* Changing this pointer should be protected using #catalog_service_mutex_. Note that changes
   * within the catalog service may still happen without the mutex being locked. They should be
   * protected separately. */
  std::unique_ptr<AssetCatalogService> catalog_service_;
  Mutex catalog_service_mutex_;

  std::optional<eAssetImportMethod> import_method_;
  /** Assets owned by this library may be imported with a different method than set in
   * #import_method_ above, it's just a default. */
  bool may_override_import_method_ = false;

  bool use_relative_path_ = true;

  bCallbackFuncStore on_save_callback_store_{};

 public:
  /* Controlled by #ed::asset::catalogs_set_save_catalogs_when_file_is_saved,
   * for managing the "Save Catalog Changes" in the quit-confirmation dialog box. */
  static bool save_catalogs_when_file_is_saved;

  friend class AssetLibraryService;
  friend class AssetRepresentation;

  /**
   * \param name: The name this asset library will be displayed in the UI as. Will also be used as
   *              a weak way to identify an asset library (e.g. by #AssetWeakReference). Make sure
   *              this is set for any custom (not builtin) asset library. That is,
   *              #ASSET_LIBRARY_CUSTOM ones.
   * \param root_path: If this is an asset library on disk, the top-level directory path.
   */
  AssetLibrary(eAssetLibraryType library_type, StringRef name = "", StringRef root_path = "");
  virtual ~AssetLibrary();

  /**
   * Execute \a fn for every asset library that is loaded. The asset library is passed to the
   * \a fn call.
   *
   * \param skip_all_library: When true, the \a fn will also be executed for the "All" asset
   *                          library. This is just a combination of the other ones, so usually
   *                          iterating over it is redundant.
   */
  static void foreach_loaded(FunctionRef<void(AssetLibrary &)> fn, bool include_all_library);

  /**
   * Get the #AssetLibraryReference referencing this library. This can fail for custom libraries,
   * which have too look up their #bUserAssetLibrary. It will not return a value for values that
   * were loaded directly through a path.
   */
  virtual std::optional<AssetLibraryReference> library_reference() const = 0;

  void load_or_reload_catalogs();

  AssetCatalogService &catalog_service() const;

  /**
   * Create a representation of an asset to be considered part of this library. Once the
   * representation is not needed anymore, it must be freed using #remove_asset(), or there will be
   * leaking that's only cleared when the library storage is destructed (typically on exit or
   * loading a different file).
   *
   * \param relative_asset_path: The path of the asset relative to the asset library root. With
   *                             this the asset must be uniquely identifiable within the asset
   *                             library.
   * \return A weak pointer to the new asset representation. The caller needs to keep some
   *         reference stored to be able to call #remove_asset(). This would be dangling once the
   *         asset library is destructed, so a weak pointer should be used to reference it.
   */
  std::weak_ptr<AssetRepresentation> add_external_asset(StringRef relative_asset_path,
                                                        StringRef name,
                                                        int id_type,
                                                        std::unique_ptr<AssetMetaData> metadata);
  /** See #AssetLibrary::add_external_asset(). */
  std::weak_ptr<AssetRepresentation> add_local_id_asset(ID &id);
  /**
   * Remove an asset from the library that was added using #add_external_asset() or
   * #add_local_id_asset(). Can usually be expected to be constant time complexity (worst case may
   * differ).
   * \note This is safe to call if \a asset is freed (dangling reference), will not perform any
   *       change then.
   * \return True on success, false if the asset couldn't be found inside the library (also the
   *         case when the reference is dangling).
   */
  bool remove_asset(AssetRepresentation &asset);

  /**
   * Remap ID pointers for local ID assets, see #BKE_lib_remap.hh. When an ID pointer would be
   * mapped to null (typically when an ID gets removed), the asset is removed, because we don't
   * support such empty/null assets.
   */
  void remap_ids_and_remove_invalid(const blender::bke::id::IDRemapper &mappings);

  /**
   * Update `catalog_simple_name` by looking up the asset's catalog by its ID.
   *
   * No-op if the catalog cannot be found. This could be the kind of "the
   * catalog definition file is corrupt/lost" scenario that the simple name is
   * meant to help recover from.
   */
  void refresh_catalog_simplename(AssetMetaData *asset_data);

  void on_blend_save_handler_register();
  void on_blend_save_handler_unregister();

  void on_blend_save_post(Main *bmain, PointerRNA **pointers, int num_pointers);

  std::string resolve_asset_weak_reference_to_full_path(const AssetWeakReference &asset_reference);

  eAssetLibraryType library_type() const;
  StringRefNull name() const;
  StringRefNull root_path() const;

 protected:
  /** Load catalogs that have changed on disk. */
  virtual void refresh_catalogs();
};

Vector<AssetLibraryReference> all_valid_asset_library_refs();

AssetLibraryReference all_library_reference();
AssetLibraryReference current_file_library_reference();
void all_library_reload_catalogs_if_dirty();

}  // namespace blender::asset_system

/**
 * Load the data for an asset library, but not the asset representations themselves (loading these
 * is currently not done in the asset system).
 *
 * For the "All" asset library (#ASSET_LIBRARY_ALL), every other known asset library will be
 * loaded as well. So a call to #AssetLibrary::foreach_loaded() can be expected to iterate over all
 * libraries.
 *
 * \warning Catalogs are reloaded, invalidating catalog pointers. Do not store catalog pointers,
 *          store CatalogIDs instead and lookup the catalog where needed.
 */
blender::asset_system::AssetLibrary *AS_asset_library_load(
    const Main *bmain, const AssetLibraryReference &library_reference);

std::string AS_asset_library_root_path_from_library_ref(
    const AssetLibraryReference &library_reference);

/**
 * Try to find an appropriate location for an asset library root from a file or directory path.
 * Does not check if \a input_path exists.
 *
 * The design is made to find an appropriate asset library path from a .blend file path, but
 * technically works with any file or directory as \a input_path.
 * Design is:
 * * If \a input_path lies within a known asset library path (i.e. an asset library registered in
 *   the Preferences), return the asset library path.
 * * Otherwise, if \a input_path has a parent path, return the parent path (e.g. to use the
 *   directory a .blend file is in as asset library root).
 * * If \a input_path is empty or doesn't have a parent path (e.g. because a .blend wasn't saved
 *   yet), there is no suitable path. The caller has to decide how to handle this case.
 *
 * \param r_library_path: The returned asset library path with a trailing slash, or an empty string
 *                        if no suitable path is found. Assumed to be a buffer of at least
 *                        #FILE_MAXDIR bytes.
 *
 * \return True if the function could find a valid, that is, a non-empty path to return in \a
 *         r_library_path.
 */
std::string AS_asset_library_find_suitable_root_path_from_path(blender::StringRefNull input_path);

/**
 * Uses the current location on disk of the file represented by \a bmain as input to
 * #AS_asset_library_find_suitable_root_path_from_path(). Refer to it for a design
 * description.
 *
 * \return True if the function could find a valid, that is, a non-empty path to return in \a
 *         r_library_path. If \a bmain wasn't saved into a file yet, the return value will be
 *         false.
 */
std::string AS_asset_library_find_suitable_root_path_from_main(const Main *bmain);

/**
 * Force clearing of all asset library data. After calling this, new asset libraries can be loaded
 * just as usual using #AS_asset_library_load(), no init or other setup is needed.
 *
 * Does not need to be called on exit, this is handled internally.
 */
void AS_asset_libraries_exit();

/**
 * Return the #AssetLibrary rooted at the given directory path.
 *
 * Will return the same pointer for repeated calls, until another blend file is loaded.
 *
 * To get the in-memory-only "current file" asset library, pass an empty path.
 */
blender::asset_system::AssetLibrary *AS_asset_library_load_from_directory(
    const char *name, const char *library_dirpath);

/** Return whether any loaded AssetLibrary has unsaved changes to its catalogs. */
bool AS_asset_library_has_any_unsaved_catalogs();

/**
 * An asset library can include local IDs (IDs in the current file). Their pointers need to be
 * remapped on change (or assets removed as IDs gets removed).
 */
void AS_asset_library_remap_ids(const blender::bke::id::IDRemapper &mappings);

/**
 * Attempt to resolve a full path to an asset based on the currently available (not necessary
 * loaded) asset libraries, and split it into it's directory, ID group and ID name components. The
 * path is not guaranteed to exist on disk. On failure to resolve the reference, return arguments
 * will point to null.
 *
 * \note Only works for asset libraries on disk and the "Current File" one (others can't be
 *       resolved).
 *
 * \param r_path_buffer: Buffer to hold the result in on success. Will be the full path with null
 *                       terminators instead of slashes separating the directory, group and name
 *                       components. Must be at least #FILE_MAX_LIBEXTRA long.
 * \param r_dir: Returns the .blend file path with native slashes on success. Optional (passing
 *               null is allowed). For the "Current File" library this will be empty.
 * \param r_group: Returns the ID group such as "Object", "Material" or "Brush". Optional (passing
 *                 null is allowed).
 * \param r_name: Returns the ID name on success. Optional (passing null is allowed).
 */
void AS_asset_full_path_explode_from_weak_ref(const AssetWeakReference *asset_reference,
                                              char r_path_buffer[/*FILE_MAX_LIBEXTRA*/ 1282],
                                              char **r_dir,
                                              char **r_group,
                                              char **r_name);

/**
 * Updates the default import method for asset libraries based on
 * #U.experimental.no_data_block_packing.
 */
void AS_asset_library_import_method_ensure_valid(Main &bmain);
/**
 * This is not done as part of #AS_asset_library_import_method_ensure_valid because it changes
 * run-time data only and does not need to happen during versioning (also it appears to break tests
 * when run during versioning).
 */
void AS_asset_library_essential_import_method_update();
