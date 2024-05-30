/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
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
