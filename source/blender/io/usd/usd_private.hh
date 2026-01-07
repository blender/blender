/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Functions shared between USD and Hydra, that are private to the USD module. */

#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/usd/common.h>

#include <string>

#include "usd.hh"

namespace blender {

struct Depsgraph;
struct Image;
struct ImageUser;
struct Scene;

namespace io::usd {

pxr::UsdStageRefPtr export_to_stage(const USDExportParams &params,
                                    Depsgraph *depsgraph,
                                    const char *filepath);

std::string image_cache_file_path();
std::string get_image_cache_file(const std::string &file_name, bool mkdir = true);
std::string cache_image_color(const float color[4]);

/** Result from converting world shader nodes to dome light parameters. */
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

};  // namespace io::usd

}  // namespace blender
