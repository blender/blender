/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

struct ID;
struct Main;
namespace blender::asset_system {
class AssetRepresentation;
}

namespace blender::ed::asset {

/**
 * If the asset already has a corresponding local #ID, return it. Otherwise, link or append the
 * asset's data-block, using "Append & Reuse" if the method is unspecified.
 *
 * \param import_method: Overrides library's default importing method.
 * If not set and the library has no default, #ASSET_IMPORT_APPEND_REUSE will be used.
 */
ID *asset_local_id_ensure_imported(
    Main &bmain,
    const asset_system::AssetRepresentation &asset,
    const std::optional<eAssetImportMethod> import_method = std::nullopt);

}  // namespace blender::ed::asset
