/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * The public API for assets is defined in dedicated headers. This is a utility file that just
 * includes all of these.
 */

#pragma once

/* Barely anything here. Just general editor level functions. Actual asset level code is in
 * dedicated headers. */

#include "../asset/ED_asset_catalog.hh"           // IWYU pragma: export
#include "../asset/ED_asset_library.hh"           // IWYU pragma: export
#include "../asset/ED_asset_list.hh"              // IWYU pragma: export
#include "../asset/ED_asset_mark_clear.hh"        // IWYU pragma: export
#include "../asset/ED_asset_temp_id_consumer.hh"  // IWYU pragma: export
#include "../asset/ED_asset_type.hh"              // IWYU pragma: export

#include "../asset/ED_asset_filter.hh"  // IWYU pragma: export
#include "../asset/ED_asset_import.hh"  // IWYU pragma: export

/** From UI_resources.hh. */
using BIFIconID = int;

struct PointerRNA;
struct uiTooltipData;

namespace blender::ed::asset {

void asset_tooltip(const asset_system::AssetRepresentation &asset,
                   uiTooltipData &tip,
                   bool include_name = true);

BIFIconID asset_preview_icon_id(const asset_system::AssetRepresentation &asset);
BIFIconID asset_preview_or_icon(const asset_system::AssetRepresentation &asset);

void operatortypes_asset();

/**
 * The PointerRNA is expected to have an enum called "asset_library_reference".
 */
const bUserAssetLibrary *get_asset_library_from_opptr(PointerRNA &ptr);
/**
 * The PointerRNA is expected to have an enum called "asset_library_reference".
 */
AssetLibraryReference get_asset_library_ref_from_opptr(PointerRNA &ptr);

/**
 * For each catalog of the given bUserAssetLibrary call `visit_fn`.
 * \param edit_text: If that text is not empty, and not matching an existing catalog path
 * `visit_fn` will be called with that text and the icon ICON_ADD.
 */
void visit_library_catalogs_catalog_for_search(
    const Main &bmain,
    const AssetLibraryReference lib,
    StringRef edit_text,
    const FunctionRef<void(StringPropertySearchVisitParams)> visit_fn);

}  // namespace blender::ed::asset
