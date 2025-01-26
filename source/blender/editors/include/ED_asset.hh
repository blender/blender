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
#include "../asset/ED_asset_handle.hh"            // IWYU pragma: export
#include "../asset/ED_asset_library.hh"           // IWYU pragma: export
#include "../asset/ED_asset_list.hh"              // IWYU pragma: export
#include "../asset/ED_asset_mark_clear.hh"        // IWYU pragma: export
#include "../asset/ED_asset_temp_id_consumer.hh"  // IWYU pragma: export
#include "../asset/ED_asset_type.hh"              // IWYU pragma: export

#include "../asset/ED_asset_filter.hh"  // IWYU pragma: export
#include "../asset/ED_asset_import.hh"  // IWYU pragma: export

struct PointerRNA;

namespace blender::ed::asset {

std::string asset_tooltip(const asset_system::AssetRepresentation &asset,
                          bool include_name = true);

void operatortypes_asset();

/**
 * The PointerRNA is expected to have an enum called "asset_library_reference".
 */
const bUserAssetLibrary *get_asset_library_from_opptr(PointerRNA &ptr);

/**
 * For each catalog of the given bUserAssetLibrary call `visit_fn`.
 */
void visit_library_catalogs_catalog_for_search(
    const Main &bmain,
    const AssetLibraryReference lib,
    const StringRef edit_text,
    const FunctionRef<void(StringPropertySearchVisitParams)> visit_fn);

}  // namespace blender::ed::asset
