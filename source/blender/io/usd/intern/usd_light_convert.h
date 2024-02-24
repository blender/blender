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
 * The Original Code is Copyright (C) 2021 NVIDIA Corporation.
 * All rights reserved.
 */
#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdLux/domeLight.h>

struct Light;
struct Main;
struct Scene;

namespace blender::io::usd {

struct USDExportParams;
struct USDImportParams;

struct ImportSettings;

float nits_to_energy_scale_factor(const Light *light,
                                  float meters_per_unit,
                                  float radius_scale = 1.0f);

void world_material_to_dome_light(const USDExportParams &params,
                                  const Scene *scene,
                                  pxr::UsdStageRefPtr stage);

void dome_light_to_world_material(const USDImportParams &params,
                                  const ImportSettings &settings,
                                  Scene *scene,
                                  Main *bmain,
                                  const pxr::UsdLuxDomeLight &dome_light,
                                  const double time = 0.0);

}  // namespace blender::io::usd
