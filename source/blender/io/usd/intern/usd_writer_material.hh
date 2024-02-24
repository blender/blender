/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/pxr.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>

#include <string>

struct bNode;
struct bNodeTree;
struct Material;
struct ReportList;

namespace blender::io::usd {

template<typename T>
T usd_define_or_over(pxr::UsdStageRefPtr stage, pxr::SdfPath path, bool as_overs = false)
{
  return (as_overs) ? T(stage->OverridePrim(path)) : T::Define(stage, path);
}

struct USDExporterContext;
struct USDExportParams;

/* Create USDMaterial from Blender material.
 *
 * \param default_uv: used as the default UV set name sampled by the `primvar`
 * reader shaders generated for image texture nodes that don't have an attached UVMap node.
 */
pxr::UsdShadeMaterial create_usd_material(const USDExporterContext &usd_export_context,
                                          pxr::SdfPath usd_path,
                                          Material *material,
                                          const std::string &active_uv,
                                          ReportList *reports);

/* Returns a USDPreviewSurface token name for a given Blender shader Socket name,
 * or an empty TfToken if the input name is not found in the map. */
const pxr::TfToken token_for_input(const char *input_name);

void export_texture(bNode *node,
                    const pxr::UsdStageRefPtr stage,
                    const bool allow_overwrite = false,
                    ReportList *reports = nullptr);

std::string get_tex_image_asset_filepath(bNode *node,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params);

std::string get_tex_image_asset_filepath(const std::string &asset_path,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params);

}  // namespace blender::io::usd
