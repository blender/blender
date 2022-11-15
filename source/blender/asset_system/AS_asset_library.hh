/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "DNA_asset_types.h"

#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "AS_asset_catalog.hh"
#include "BKE_callbacks.h"

#include <memory>

struct AssetLibrary;
struct AssetLibraryReference;
struct AssetMetaData;
struct Main;

namespace blender::asset_system {

class AssetRepresentation;

/**
 * AssetLibrary provides access to an asset library's data.
 *
 * The asset library contains catalogs and storage for asset representations. It could be extended
 * to also include asset indexes and more.
 */
struct AssetLibrary {
  /* Controlled by #ED_asset_catalogs_set_save_catalogs_when_file_is_saved,
   * for managing the "Save Catalog Changes" in the quit-confirmation dialog box. */
  static bool save_catalogs_when_file_is_saved;

  std::unique_ptr<AssetCatalogService> catalog_service;

  AssetLibrary();
  ~AssetLibrary();

  void load_catalogs(StringRefNull library_root_directory);

  /** Load catalogs that have changed on disk. */
  void refresh();

  /**
   * Create a representation of an asset to be considered part of this library. Once the
   * representation is not needed anymore, it must be freed using #remove_asset(), or there will be
   * leaking that's only cleared when the library storage is destructed (typically on exit or
   * loading a different file).
   */
  AssetRepresentation &add_external_asset(StringRef name, std::unique_ptr<AssetMetaData> metadata);
  AssetRepresentation &add_local_id_asset(ID &id);
  /** Remove an asset from the library that was added using #add_external_asset() or
   * #add_local_id_asset().
   * \return True on success, false if the asset couldn't be found inside the library. */
  bool remove_asset(AssetRepresentation &asset);

  /**
   * Update `catalog_simple_name` by looking up the asset's catalog by its ID.
   *
   * No-op if the catalog cannot be found. This could be the kind of "the
   * catalog definition file is corrupt/lost" scenario that the simple name is
   * meant to help recover from. */
  void refresh_catalog_simplename(AssetMetaData *asset_data);

  void on_blend_save_handler_register();
  void on_blend_save_handler_unregister();

  void on_blend_save_post(Main *bmain, PointerRNA **pointers, int num_pointers);

  void remap_ids(struct IDRemapper &mappings);

 private:
  bCallbackFuncStore on_save_callback_store_{};

  /** Storage for assets (better said their representations) that are considered to be part of this
   * library. Assets are not automatically loaded into this when loading an asset library. Assets
   * have to be loaded externally and added to this storage via #add_external_asset() or
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
  Set<std::unique_ptr<AssetRepresentation>> asset_storage_;

  std::optional<int> find_asset_index(const AssetRepresentation &asset);
};

Vector<AssetLibraryReference> all_valid_asset_library_refs();

}  // namespace blender::asset_system

blender::asset_system::AssetLibrary *AS_asset_library_load(
    const Main *bmain, const AssetLibraryReference &library_reference);

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
std::string AS_asset_library_find_suitable_root_path_from_main(const struct Main *bmain);

blender::asset_system::AssetCatalogService *AS_asset_library_get_catalog_service(
    const ::AssetLibrary *library);
blender::asset_system::AssetCatalogTree *AS_asset_library_get_catalog_tree(
    const ::AssetLibrary *library);
