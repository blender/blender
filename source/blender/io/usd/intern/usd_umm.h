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
 *
 */

#pragma once

#ifdef WITH_PYTHON

#  include <pxr/usd/usdShade/material.h>

struct Material;
struct USDImportParams;

namespace blender::io::usd {

struct USDExporterContext;

bool umm_module_loaded();

bool umm_import_material(const USDImportParams &import_params,
                         Material *mtl,
                         const pxr::UsdShadeMaterial &usd_material,
                         const std::string &render_context);

bool umm_export_material(const USDExporterContext &usd_export_context,
                         const Material *mtl,
                         const pxr::UsdShadeMaterial &usd_material,
                         const std::string &render_context);

}  // namespace blender::io::usd

#endif
