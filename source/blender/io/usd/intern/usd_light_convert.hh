/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/common.h>

struct Main;
struct Scene;

namespace blender::io::usd {

struct USDExportParams;
struct USDImportParams;

/* This struct contains all DomeLight attribute needed to
 * create a world environment */
struct USDImportDomeLightData {
  float intensity;
  pxr::GfVec3f color;
  pxr::SdfAssetPath tex_path;
  pxr::TfToken pole_axis;

  bool has_color;
  bool has_tex;
};

/**
 * If the Blender scene has an environment texture,
 * export it as a USD dome light.
 */
void world_material_to_dome_light(const USDExportParams &params,
                                  const Scene *scene,
                                  pxr::UsdStageRefPtr stage);

void dome_light_to_world_material(const USDImportParams &params,
                                  Scene *scene,
                                  Main *bmain,
                                  const USDImportDomeLightData &dome_light_data,
                                  const pxr::UsdPrim &prim,
                                  const pxr::UsdTimeCode time = 0.0);

}  // namespace blender::io::usd
