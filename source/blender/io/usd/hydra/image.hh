/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

namespace blender {

struct Main;
struct Scene;
struct Image;
struct ImageUser;

namespace io::hydra {

std::string cache_or_get_image_file(Main *bmain, Scene *Scene, Image *image, ImageUser *iuser);

}  // namespace io::hydra
}  // namespace blender
