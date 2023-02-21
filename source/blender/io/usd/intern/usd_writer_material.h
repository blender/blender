/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/pxr.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>

#include <string>

struct bNode;
struct bNodeTree;
struct Material;
struct USDExportParams;
struct Material;

namespace blender::io::usd {

template<typename T>
T usd_define_or_over(pxr::UsdStageRefPtr stage, pxr::SdfPath path, bool as_overs = false)
{
  return (as_overs) ? T(stage->OverridePrim(path)) : T::Define(stage, path);
}

struct USDExporterContext;

void create_usd_cycles_material(pxr::UsdStageRefPtr a_stage,
                                bNodeTree *ntree,
                                pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params);
void create_usd_cycles_material(pxr::UsdStageRefPtr a_stage,
                                Material *material,
                                pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params);
void create_mdl_material(const USDExporterContext &usd_export_context,
                         Material *material,
                         pxr::UsdShadeMaterial &usd_material);
/* Returns a USDPreviewSurface token name for a given Blender shader Socket name,
 * or an empty TfToken if the input name is not found in the map. */
const pxr::TfToken token_for_input(const char *input_name);

/**
 * Entry point to create an approximate USD Preview Surface network from a Cycles node graph.
 * Due to the limited nodes in the USD Preview Surface specification, only the following nodes
 * are supported:
 * - UVMap
 * - Texture Coordinate
 * - Image Texture
 * - Principled BSDF
 * More may be added in the future.
 *
 * \param default_uv: used as the default UV set name sampled by the `primvar`
 * reader shaders generated for image texture nodes that don't have an attached UVMap node.
 */
void create_usd_preview_surface_material(const USDExporterContext &usd_export_context,
                                         Material *material,
                                         pxr::UsdShadeMaterial &usd_material,
                                         const std::string &default_uv = "");

/* Entry point to create USD Shade Material network from Blender viewport display settings. */
void create_usd_viewport_material(const USDExporterContext &usd_export_context,
                                  Material *material,
                                  pxr::UsdShadeMaterial &usd_material);

void export_texture(bNode *node,
                    const pxr::UsdStageRefPtr stage,
                    const bool allow_overwrite = false);

std::string get_tex_image_asset_path(bNode *node,
                                     const pxr::UsdStageRefPtr stage,
                                     const USDExportParams &export_params);

std::string get_tex_image_asset_path(const std::string &asset_path,
                                     const pxr::UsdStageRefPtr stage,
                                     const USDExportParams &export_params);

void export_textures(const Material *material,
                     const pxr::UsdStageRefPtr stage,
                     bool allow_overwrite = false);

}  // namespace blender::io::usd

