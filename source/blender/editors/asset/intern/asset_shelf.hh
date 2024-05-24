/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "BLI_function_ref.hh"

struct ARegion;
struct ARegionType;
struct AssetLibraryReference;
struct AssetShelf;
struct AssetShelfType;
struct AssetShelfSettings;
struct bContext;
struct BlendDataReader;
struct BlendWriter;
struct RegionAssetShelf;
struct SpaceType;
struct uiLayout;

namespace blender::asset_system {
class AssetCatalogPath;
}

namespace blender::ed::asset::shelf {

constexpr short DEFAULT_TILE_SIZE = 64;

void build_asset_view(uiLayout &layout,
                      const AssetLibraryReference &library_ref,
                      const AssetShelf &shelf,
                      const bContext &C,
                      const ARegion &region);

void catalog_selector_panel_register(ARegionType *region_type);
void popover_panel_register(ARegionType *region_type);

AssetShelf *active_shelf_from_context(const bContext *C);

void send_redraw_notifier(const bContext &C);

AssetShelfType *ensure_shelf_has_type(AssetShelf &shelf);
AssetShelf *create_shelf_from_type(AssetShelfType &type);

void library_selector_draw(const bContext *C, uiLayout *layout, AssetShelf &shelf);

/**
 * Deep-copies \a shelf_regiondata into newly allocated memory. Must be freed using
 * #regiondata_free().
 */
RegionAssetShelf *regiondata_duplicate(const RegionAssetShelf *shelf_regiondata);
/** Frees the contained data and \a shelf_regiondata itself. */
void regiondata_free(RegionAssetShelf *shelf_regiondata);
void regiondata_blend_write(BlendWriter *writer, const RegionAssetShelf *shelf_regiondata);
void regiondata_blend_read_data(BlendDataReader *reader, RegionAssetShelf **shelf_regiondata);

void settings_blend_write(BlendWriter *writer, const AssetShelfSettings &settings);
void settings_blend_read_data(BlendDataReader *reader, AssetShelfSettings &settings);

void settings_set_active_catalog(AssetShelfSettings &settings,
                                 const asset_system::AssetCatalogPath &path);
void settings_set_all_catalog_active(AssetShelfSettings &settings);
bool settings_is_active_catalog(const AssetShelfSettings &settings,
                                const asset_system::AssetCatalogPath &path);
bool settings_is_all_catalog_active(const AssetShelfSettings &settings);
/**
 * Clears the list of enabled catalogs in either the Preferences (if any) or the asset shelf
 * settings (if any), depending on the #ASSET_SHELF_TYPE_FLAG_STORE_CATALOGS_IN_PREFS flag.
 */
void settings_clear_enabled_catalogs(AssetShelf &shelf);
bool settings_is_catalog_path_enabled(const AssetShelf &shelf,
                                      const asset_system::AssetCatalogPath &path);
void settings_set_catalog_path_enabled(AssetShelf &shelf,
                                       const asset_system::AssetCatalogPath &path);

void settings_foreach_enabled_catalog_path(
    const AssetShelf &shelf,
    FunctionRef<void(const asset_system::AssetCatalogPath &catalog_path)> fn);

}  // namespace blender::ed::asset::shelf
