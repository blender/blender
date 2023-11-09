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

struct AssetLibrary;
struct bScreen;
struct uiLayout;

namespace blender::asset_system {
class AssetCatalogTreeItem;
class AssetLibrary;
class AssetRepresentation;
}  // namespace blender::asset_system

namespace blender::ed::asset {

/**
 * Some code needs to pass catalog paths to context and for this they need persistent pointers to
 * the paths. Rather than keeping some local path storage, get a pointer into the asset system
 * directly, which is persistent until the library is reloaded and can safely be held by context.
 */
PointerRNA persistent_catalog_path_rna_pointer(const bScreen &owner_screen,
                                               const asset_system::AssetLibrary &library,
                                               const asset_system::AssetCatalogTreeItem &item);

void draw_menu_for_catalog(const bScreen &owner_screen,
                           const asset_system::AssetLibrary &library,
                           const asset_system::AssetCatalogTreeItem &item,
                           StringRefNull menu_name,
                           uiLayout &layout);

void operator_asset_reference_props_set(const asset_system::AssetRepresentation &asset,
                                        PointerRNA &ptr);
void operator_asset_reference_props_register(StructRNA &srna);

/**
 * Load all asset libraries to find an asset from the #operator_asset_reference_props_register
 * properties. The loading happens in the background, so there may be no result immediately. In
 * that case an "Asset loading is unfinished" report is added.
 *
 * \note Does not check asset type or meta data.
 */
const asset_system::AssetRepresentation *operator_asset_reference_props_get_asset_from_all_library(
    const bContext &C, PointerRNA &ptr, ReportList *reports);

}  // namespace blender::ed::asset
