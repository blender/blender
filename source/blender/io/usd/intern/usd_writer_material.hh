/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/usd/usdShade/material.h>

#include <string>

struct bNode;
struct Material;
struct ReportList;

namespace blender::io::usd {

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

/* Export the given texture node's image to a 'textures' directory in the given stage's
 * export path. */
void export_texture(bNode *node,
                    const pxr::UsdStageRefPtr stage,
                    const bool allow_overwrite = false,
                    ReportList *reports = nullptr);

/* Gets an asset path for the given texture image node. The resulting path
 * may be absolute, relative to the USD file, or in a 'textures' directory
 * in the same directory as the USD file, depending on the export parameters.
 * The filename is typically the image filepath but might also be automatically
 * generated based on the image name for in-memory textures when exporting textures.
 * This function may return an empty string if the image does not have a filepath
 * assigned and no asset path could be determined. */
std::string get_tex_image_asset_filepath(bNode *node,
                                         const pxr::UsdStageRefPtr stage,
                                         const USDExportParams &export_params);

}  // namespace blender::io::usd
