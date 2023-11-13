/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

struct Main;
struct Scene;
struct Image;
struct ImageUser;

namespace blender::io::hydra {

std::string image_cache_file_path();

std::string cache_or_get_image_file(Main *bmain, Scene *Scene, Image *image, ImageUser *iuser);
std::string cache_image_color(float color[4]);

}  // namespace blender::io::hydra
