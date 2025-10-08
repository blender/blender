/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include <cstring>

#include "DNA_color_types.h"
#include "DNA_image_types.h"

namespace blender::image_engine {

/**
 * ImageUsage contains data of the image and image user to identify changes that require a rebuild
 * the texture slots.
 */
struct ImageUsage {
  /** Render pass of the image that is used. */
  short pass = 0;
  /** Layer of the image that is used. */
  short layer = 0;
  /** View of the image that is used. */
  short view = 0;

  ColorManagedColorspaceSettings colorspace_settings;
  /** IMA_ALPHA_* */
  char alpha_mode;
  bool last_tile_drawing;

  const void *last_image = nullptr;
  const void *last_scene = nullptr;

  ImageUsage() = default;
  ImageUsage(const ::Image *image, const ::ImageUser *image_user, bool do_tile_drawing)
  {
    pass = image_user ? image_user->pass : 0;
    layer = image_user ? image_user->layer : 0;
    view = image_user ? image_user->multi_index : 0;
    colorspace_settings = image->colorspace_settings;
    alpha_mode = image->alpha_mode;
    last_image = static_cast<const void *>(image);
    last_scene = image_user ? static_cast<const void *>(image_user->scene) : nullptr;
    last_tile_drawing = do_tile_drawing;
  }

  bool operator==(const ImageUsage &other) const
  {
    return memcmp(this, &other, sizeof(ImageUsage)) == 0;
  }
  bool operator!=(const ImageUsage &other) const
  {
    return !(*this == other);
  }
};

}  // namespace blender::image_engine
