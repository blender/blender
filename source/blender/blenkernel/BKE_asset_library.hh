/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#ifndef __cplusplus
#  error This is a C++-only header file. Use BKE_asset_library.h instead.
#endif

#include "BKE_asset_library.h"

#include "BKE_asset_catalog.hh"
#include "BKE_callbacks.h"

#include <memory>

namespace blender::bke {

/**
 * AssetLibrary provides access to an asset library's data.
 * For now this is only for catalogs, later this can be expanded to indexes/caches/more.
 */
struct AssetLibrary {
  /* Controlled by #ED_asset_catalogs_set_save_catalogs_when_file_is_saved,
   * for managing the "Save Catalog Changes" in the quit-confirmation dialog box. */
  static bool save_catalogs_when_file_is_saved;

  std::unique_ptr<AssetCatalogService> catalog_service;

  AssetLibrary();
  ~AssetLibrary();

  void load(StringRefNull library_root_directory);

  /** Load catalogs that have changed on disk. */
  void refresh();

  /**
   * Update `catalog_simple_name` by looking up the asset's catalog by its ID.
   *
   * No-op if the catalog cannot be found. This could be the kind of "the
   * catalog definition file is corrupt/lost" scenario that the simple name is
   * meant to help recover from. */
  void refresh_catalog_simplename(struct AssetMetaData *asset_data);

  void on_blend_save_handler_register();
  void on_blend_save_handler_unregister();

  void on_blend_save_post(struct Main *, struct PointerRNA **pointers, int num_pointers);

 private:
  bCallbackFuncStore on_save_callback_store_{};
};

}  // namespace blender::bke

blender::bke::AssetCatalogService *BKE_asset_library_get_catalog_service(
    const ::AssetLibrary *library);
blender::bke::AssetCatalogTree *BKE_asset_library_get_catalog_tree(const ::AssetLibrary *library);
