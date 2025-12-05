/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include <optional>

struct ID;
struct Main;
struct ReportList;
struct Scene;
struct View3D;
struct ViewLayer;
namespace blender::asset_system {
class AssetRepresentation;
}

namespace blender::ed::asset {

struct ImportInstantiateContext {
  Scene *scene;
  ViewLayer *view_layer;
  View3D *view3d;
};

/**
 * If the asset already has a corresponding local #ID, return it. Otherwise, link or append the
 * asset's data-block, using "Append & Reuse" if the method is unspecified.
 *
 * \note This can return null! Importing can fail if the asset was deleted or moved since the asset
 * library was loaded.
 *
 * \param import_method: Overrides library's default importing method.
 * If not set and the library has no default, #ASSET_IMPORT_APPEND_REUSE will be used.
 */
ID *asset_local_id_ensure_imported(
    Main &bmain,
    const asset_system::AssetRepresentation &asset,
    int flags = 0, /* #eFileSel_Params_Flag + #eBLOLibLinkFlags */
    const std::optional<eAssetImportMethod> import_method = std::nullopt,
    const std::optional<ImportInstantiateContext> instantiate_context = std::nullopt,
    ReportList *reports = nullptr);

}  // namespace blender::ed::asset
