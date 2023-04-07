/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "BLI_function_ref.hh"

struct ARegion;
struct AssetLibraryReference;
struct AssetShelfSettings;
struct bContext;
struct uiBlock;
struct uiLayout;

namespace blender::asset_system {
class AssetCatalogPath;
}

namespace blender::ed::asset::shelf {

void build_asset_view(uiLayout &layout,
                      const AssetLibraryReference &library_ref,
                      const AssetShelfSettings *shelf_settings,
                      const bContext &C,
                      ARegion &region);

uiBlock *catalog_selector_block_draw(bContext *C, ARegion *region, void * /*arg1*/);

AssetShelfSettings *settings_from_context(const bContext *C);

void send_redraw_notifier(const bContext &C);

void settings_clear_enabled_catalogs(AssetShelfSettings &shelf_settings);
void settings_set_active_catalog(AssetShelfSettings &shelf_settings,
                                 const asset_system::AssetCatalogPath &path);
void settings_set_all_catalog_active(AssetShelfSettings &shelf_settings);
bool settings_is_active_catalog(const AssetShelfSettings &shelf_settings,
                                const asset_system::AssetCatalogPath &path);
bool settings_is_all_catalog_active(const AssetShelfSettings &shelf_settings);
bool settings_is_catalog_path_enabled(const AssetShelfSettings &shelf_settings,
                                      const asset_system::AssetCatalogPath &path);
void settings_set_catalog_path_enabled(AssetShelfSettings &shelf_settings,
                                       const asset_system::AssetCatalogPath &path);

void settings_foreach_enabled_catalog_path(
    const AssetShelfSettings &shelf_settings,
    FunctionRef<void(const asset_system::AssetCatalogPath &catalog_path)> fn);

}  // namespace blender::ed::asset::shelf
