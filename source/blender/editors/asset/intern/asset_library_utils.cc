/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_representation.hh"

#include "BKE_context.hh"

#include "ED_asset_library.hh"
#include "ED_asset_list.hh"
#include "ED_asset_shelf.hh"

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"

#include "DNA_userdef_types.h"
#include "RNA_access.hh"
#include "WM_api.hh"

#include "AS_asset_catalog.hh"
#include "AS_asset_library.hh"

namespace blender::ed::asset {

static asset_system::AssetCatalog &library_ensure_catalog(
    asset_system::AssetLibrary &library, const asset_system::AssetCatalogPath &path)
{
  if (asset_system::AssetCatalog *catalog = library.catalog_service().find_catalog_by_path(path)) {
    return *catalog;
  }
  return *library.catalog_service().create_catalog(path);
}

/* Suppress warning for GCC-14.2. This isn't a dangling reference
 * because the #asset_system::AssetLibrary owns the returned value. */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

blender::asset_system::AssetCatalog &library_ensure_catalogs_in_path(
    asset_system::AssetLibrary &library, const blender::asset_system::AssetCatalogPath &path)
{
  /* Adding multiple catalogs in a path at a time with #AssetCatalogService::create_catalog()
   * doesn't work; add each potentially new catalog in the hierarchy manually here. */
  asset_system::AssetCatalogPath parent = "";
  path.iterate_components([&](StringRef component_name, bool /*is_last_component*/) {
    library_ensure_catalog(library, parent / component_name);
    parent = parent / component_name;
  });
  return *library.catalog_service().find_catalog_by_path(path);
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif

AssetLibraryReference user_library_to_library_ref(const bUserAssetLibrary &user_library)
{
  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = BLI_findindex(&U.asset_libraries, &user_library);
  library_ref.type = ASSET_LIBRARY_CUSTOM;
  return library_ref;
}

void refresh_asset_library(const bContext *C, const AssetLibraryReference &library_ref)
{
  asset::list::clear(&library_ref, C);
  /* TODO: Should the all library reference be automatically cleared? */
  AssetLibraryReference all_lib_ref = asset_system::all_library_reference();
  asset::list::clear(&all_lib_ref, C);
}

void refresh_asset_library(const bContext *C, const bUserAssetLibrary &user_library)
{
  refresh_asset_library(C, user_library_to_library_ref(user_library));
}

void refresh_asset_library_from_asset(const bContext *C,
                                      const asset_system::AssetRepresentation &asset)
{
  if (std::optional<AssetLibraryReference> library_ref =
          asset.owner_asset_library().library_reference())
  {
    refresh_asset_library(C, *library_ref);
  }
}

}  // namespace blender::ed::asset
