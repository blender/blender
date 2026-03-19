/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Window icon generation for Wayland.
 * Rasterizes the full-color application SVG for a window icon.
 */

#include <algorithm>
#include <cstring>
#include <string>

#include "GHOST_Types.hh"

#include "nanosvgrast.h"

#include "wm_window_icon.hh"

extern "C" const char datatoc_blender_app_icon_svg[];

static void wm_ghost_icon_generate(const GHOST_IconGenerator * /*icon_generator*/,
                                   GHOST_IWindow * /*window*/,
                                   uint8_t *pixels,
                                   int icon_size)
{
  const size_t buffer_size = size_t(icon_size) * icon_size * 4;
  const int stride = icon_size * 4;

  /* Rasterize the Blender logo SVG into the icon buffer.
   * `nsvgParse` modifies the source string, so make a copy. */
  std::string svg_source = datatoc_blender_app_icon_svg;
  NSVGimage *image = nsvgParse(svg_source.data(), "px", 96.0f);
  /* The bundled SVG is known to be valid. */
  if (image == nullptr || image->width == 0 || image->height == 0) [[unlikely]] {
    if (image) {
      nsvgDelete(image);
    }
    memset(pixels, 0, buffer_size);
    return;
  }

  NSVGrasterizer *rast = nsvgCreateRasterizer();
  /* Rasterizer allocation should not fail for a small buffer. */
  if (rast == nullptr) [[unlikely]] {
    nsvgDelete(image);
    memset(pixels, 0, buffer_size);
    return;
  }

  /* Scale to fit the icon, centering the shorter axis. */
  const float scale = float(icon_size) / std::max(image->width, image->height);
  const float offset_x = (icon_size - image->width * scale) * 0.5f;
  const float offset_y = (icon_size - image->height * scale) * 0.5f;

  /* Clear the buffer first since the SVG may not fill the entire square. */
  memset(pixels, 0, buffer_size);

  nsvgRasterize(rast, image, offset_x, offset_y, scale, pixels, icon_size, icon_size, stride);

  nsvgDeleteRasterizer(rast);
  nsvgDelete(image);
}

const GHOST_IconGenerator wm_ghost_icon_generator = {
    /*generate_fn*/ wm_ghost_icon_generate,
    /*user_data*/ nullptr,
};
