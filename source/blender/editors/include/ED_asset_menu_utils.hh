/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Code for dealing with dynamic asset menus and passing assets to operators with RNA properties.
 */

#pragma once

#include "BLI_string_ref.hh"

#include "RNA_types.hh"

#include "WM_types.hh"

namespace blender {

using BIFIconID = int;

struct AssetWeakReference;

namespace asset_system {
class AssetCatalogTreeItem;
class AssetLibrary;
class AssetRepresentation;
}  // namespace asset_system

namespace ui {
struct Layout;
}  // namespace ui

namespace ed::asset {

void draw_menu_for_catalog(const asset_system::AssetCatalogTreeItem &item,
                           StringRefNull menu_name,
                           ui::Layout &layout);

void draw_node_menu_for_catalog(const asset_system::AssetCatalogTreeItem &item,
                                StringRefNull operator_id,
                                StringRefNull menu_name,
                                ui::Layout &layout);
void draw_asset_menu_item(const asset_system::AssetRepresentation *asset,
                          StringRefNull opname,
                          wm::OpCallContext operator_call_context,
                          ui::Layout &layout);
/**
 * Overload that allows adding the actual menu item button in a callback, with a layout prepared
 * for listing assets.
 * \param draw_item_fn: The function to insert the button. The passed layout will have the asset
 *     context pointer assigned so the context menu can show operations (like "Download Asset"),
 *     and the layout will be grayed out in case the asset needs downloading. The passed icon
 *     represents the online asset status.
 */
void draw_asset_menu_item(const asset_system::AssetRepresentation *asset,
                          ui::Layout &layout,
                          FunctionRef<void(ui::Layout &, BIFIconID)> draw_item_fn);

void operator_asset_reference_props_set(const asset_system::AssetRepresentation &asset,
                                        PointerRNA &ptr);
bool operator_asset_reference_props_is_set(PointerRNA &ptr);
void operator_asset_reference_props_register(StructRNA &srna);

const asset_system::AssetRepresentation *find_asset_from_weak_ref(
    const bContext &C, const AssetWeakReference &weak_ref, ReportList *reports);

/**
 * Load all asset libraries to find an asset from the #operator_asset_reference_props_register
 * properties. The loading happens in the background, so there may be no result immediately. In
 * that case an "Asset loading is unfinished" report is added.
 *
 * \note Does not check asset type or meta data.
 */
const asset_system::AssetRepresentation *operator_asset_reference_props_get_asset_from_all_library(
    const bContext &C, PointerRNA &ptr, ReportList *reports);

}  // namespace ed::asset
}  // namespace blender
