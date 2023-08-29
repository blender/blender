/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Functions for filtering assets.
 */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_multi_value_map.hh"

#include "AS_asset_catalog_path.hh"
#include "AS_asset_catalog_tree.hh"

struct AssetFilterSettings;
struct AssetHandle;
struct AssetLibraryReference;
struct bContext;

namespace blender::asset_system {
class AssetLibrary;
class AssetRepresentation;
}  // namespace blender::asset_system

/**
 * Compare \a asset against the settings of \a filter.
 *
 * Individual filter parameters are ORed with the asset properties. That means:
 * * The asset type must be one of the ID types filtered by, and
 * * The asset must contain at least one of the tags filtered by.
 * However for an asset to be matching it must have one match in each of the parameters. I.e. one
 * matching type __and__ at least one matching tag.
 *
 * \returns True if the asset should be visible with these filter settings (parameters match).
 * Otherwise returns false (mismatch).
 */
bool ED_asset_filter_matches_asset(const AssetFilterSettings *filter,
                                   const blender::asset_system::AssetRepresentation &asset);

namespace blender::ed::asset {

struct AssetItemTree {
  asset_system::AssetCatalogTree catalogs;
  MultiValueMap<asset_system::AssetCatalogPath, asset_system::AssetRepresentation *>
      assets_per_path;
};

asset_system::AssetCatalogTree build_filtered_catalog_tree(
    const asset_system::AssetLibrary &library,
    const AssetLibraryReference &library_ref,
    blender::FunctionRef<bool(const AssetHandle &)> is_asset_visible_fn);
AssetItemTree build_filtered_all_catalog_tree(
    const AssetLibraryReference &library_ref,
    const bContext &C,
    const AssetFilterSettings &filter_settings,
    FunctionRef<bool(const AssetMetaData &)> meta_data_filter = {});

}  // namespace blender::ed::asset
