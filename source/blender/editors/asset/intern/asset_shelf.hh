/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "BLI_function_ref.hh"

struct ARegion;
struct AssetLibraryReference;
struct AssetShelf;
struct AssetShelfSettings;
struct bContext;
struct BlendDataReader;
struct BlendWriter;
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
                      ARegion &region);

void catalog_selector_panel_register(struct ARegionType *region_type);

AssetShelf *active_shelf_from_context(const bContext *C);

void send_redraw_notifier(const bContext &C);

/**
 * Frees the contained data, not \a shelf_settings itself.
 */
void settings_free(AssetShelfSettings *settings);
void settings_blend_write(BlendWriter *writer, const AssetShelfSettings &settings);
void settings_blend_read_data(BlendDataReader *reader, AssetShelfSettings &settings);

void settings_clear_enabled_catalogs(AssetShelfSettings &settings);
void settings_set_active_catalog(AssetShelfSettings &settings,
                                 const asset_system::AssetCatalogPath &path);
void settings_set_all_catalog_active(AssetShelfSettings &settings);
bool settings_is_active_catalog(const AssetShelfSettings &settings,
                                const asset_system::AssetCatalogPath &path);
bool settings_is_all_catalog_active(const AssetShelfSettings &settings);
bool settings_is_catalog_path_enabled(const AssetShelfSettings &settings,
                                      const asset_system::AssetCatalogPath &path);
void settings_set_catalog_path_enabled(AssetShelfSettings &settings,
                                       const asset_system::AssetCatalogPath &path);

void settings_foreach_enabled_catalog_path(
    const AssetShelfSettings &settings,
    FunctionRef<void(const asset_system::AssetCatalogPath &catalog_path)> fn);

}  // namespace blender::ed::asset::shelf
