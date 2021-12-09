/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edasset
 *
 * UI/Editor level API for catalog operations, creating richer functionality than the BKE catalog
 * API provides (which this uses internally).
 *
 * Note that `ED_asset_catalog.h` is part of this API.
 */

#pragma once

#include <optional>

#include "BKE_asset_catalog.hh"

#include "BLI_string_ref.hh"

struct AssetLibrary;
namespace blender::bke {
class AssetCatalog;
}  // namespace blender::bke

blender::bke::AssetCatalog *ED_asset_catalog_add(AssetLibrary *library,
                                                 blender::StringRefNull name,
                                                 blender::StringRef parent_path = nullptr);
void ED_asset_catalog_remove(AssetLibrary *library, const blender::bke::CatalogID &catalog_id);

void ED_asset_catalog_rename(AssetLibrary *library,
                             blender::bke::CatalogID catalog_id,
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
    blender::bke::CatalogID src_catalog_id,
    std::optional<blender::bke::CatalogID> dst_parent_catalog_id = std::nullopt);
