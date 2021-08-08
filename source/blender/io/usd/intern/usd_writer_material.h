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
 * \ingroup busd
 */

#ifndef __USD_MATERIAL_H__
#define __USD_MATERIAL_H__

#include <string>

#ifdef _MSC_VER
#  define USD_INLINE static __forceinline
#else
#  define USD_INLINE static inline
#endif

#include <pxr/pxr.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>

#include "usd.h"
#include "usd_exporter_context.h"

#include <string>

struct Material;
struct bNode;
struct bNodeTree;

namespace blender::io::usd {

template<typename T>
T usd_define_or_over(pxr::UsdStageRefPtr stage, pxr::SdfPath path, bool as_overs = false)
{
  return (as_overs) ? T(stage->OverridePrim(path)) : T::Define(stage, path);
}

void create_usd_preview_surface_material(USDExporterContext const &usd_export_context_,
                                         Material *material,
                                         pxr::UsdShadeMaterial &usd_material);
void create_usd_cycles_material(pxr::UsdStageRefPtr a_stage,
                                bNodeTree *ntree,
                                pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params);
void create_usd_cycles_material(pxr::UsdStageRefPtr a_stage,
                                Material *material,
                                pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params);
void create_usd_viewport_material(USDExporterContext const &usd_export_context_,
                                  Material *material,
                                  pxr::UsdShadeMaterial &usd_material);
void create_mdl_material(const USDExporterContext &usd_export_context,
                         Material *material,
                         pxr::UsdShadeMaterial &usd_material);

void export_texture(bNode *node, const pxr::UsdStageRefPtr stage);

void export_textures(const Material *material, const pxr::UsdStageRefPtr stage);

std::string get_node_tex_image_filepath(bNode *node,
                                        const pxr::UsdStageRefPtr stage,
                                        const USDExportParams &export_params);

std::string get_texture_filepath(const std::string &tex_filepath,
                                 const pxr::UsdStageRefPtr stage,
                                 const USDExportParams &export_params);

}  // Namespace blender::io::usd

#endif /* __USD_MATERIAL_H__ */
