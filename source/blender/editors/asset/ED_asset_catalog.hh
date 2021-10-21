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
 */

#pragma once

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
