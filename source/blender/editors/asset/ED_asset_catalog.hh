/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * UI/Editor level API for catalog operations, creating richer functionality than the asset system
 * catalog API provides (which this uses internally).
 *
 * Functions can be expected to not perform any change when #ED_asset_catalogs_read_only() returns
 * true. Generally UI code should disable such functionality in this case, so these functions are
 * not called at all.
 *
 * Note that `ED_asset_catalog.h` is part of this API.
 */

#pragma once

#include <optional>

#include "AS_asset_catalog.hh"

#include "BLI_string_ref.hh"

struct AssetLibrary;
struct bScreen;

namespace blender::asset_system {
class AssetCatalogTreeItem;
}

/**
 * Returns if the catalogs of \a library are allowed to be editable, or if the UI should forbid
 * edits.
 */
[[nodiscard]] bool ED_asset_catalogs_read_only(const AssetLibrary &library);

blender::asset_system::AssetCatalog *ED_asset_catalog_add(
    AssetLibrary *library, blender::StringRefNull name, blender::StringRef parent_path = nullptr);
void ED_asset_catalog_remove(AssetLibrary *library,
                             const blender::asset_system::CatalogID &catalog_id);

void ED_asset_catalog_rename(AssetLibrary *library,
                             blender::asset_system::CatalogID catalog_id,
                             blender::StringRefNull new_name);
/**
 * Reinsert catalog identified by \a src_catalog_id as child to catalog identified by \a
 * dst_parent_catalog_id. If \a dst_parent_catalog_id is not set, the catalog is moved to the root
 * level of the tree.
 * The name of the reinserted catalog is made unique within the parent. Note that moving a catalog
 * to the same level it was before will also change its name, since the name uniqueness check isn't
 * smart enough to ignore the item to be reinserted. So the caller is expected to handle this case
 * to avoid unwanted renames.
 *
 * Nothing is done (debug builds run into an assert) if the given catalog IDs can't be identified.
 */
void ED_asset_catalog_move(
    AssetLibrary *library,
    blender::asset_system::CatalogID src_catalog_id,
    std::optional<blender::asset_system::CatalogID> dst_parent_catalog_id = std::nullopt);

namespace blender::ed::asset {

/**
 * Some code needs to pass catalog paths to context and for this they need persistent pointers to
 * the paths. Rather than keeping some local path storage, get a pointer into the asset system
 * directly, which is persistent until the library is reloaded and can safely be held by context.
 */
PointerRNA persistent_catalog_path_rna_pointer(const bScreen &owner_screen,
                                               const asset_system::AssetLibrary &library,
                                               const asset_system::AssetCatalogTreeItem &item);

}  // namespace blender::ed::asset
