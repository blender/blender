/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/common.h>

struct bNode;
struct bNodeTree;

struct Image;
struct ImageUser;
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

/**
 * Helper struct for converting world shader nodes to a dome light, used by both
 * USD and Hydra. */
struct WorldToDomeLight {
  /* Image and its transform. */
  Image *image = nullptr;
  ImageUser *iuser = nullptr;
  pxr::GfMatrix4d transform = pxr::GfMatrix4d(1.0);

  /* Multiply image by color. */
  bool mult_found = false;
  float color_mult[4]{};

  /* Fixed color. */
  bool color_found = false;
  float intensity = 0.0f;
  float color[4]{};
};

void world_material_to_dome_light(const Scene *scene, WorldToDomeLight &res);

}  // namespace blender::io::usd
