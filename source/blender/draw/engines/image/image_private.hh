/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include <optional>

#include "BKE_image.hh"

#include "image_state.hh"
#include "image_texture_info.hh"

/* Forward declarations */
extern "C" {
struct Image;
}

/* *********** LISTS *********** */

namespace blender::image_engine {

/**
 * Abstract class for a drawing mode of the image engine.
 *
 * The drawing mode decides how to draw the image on the screen. Each way how to draw would have
 * its own subclass. For now there is only a single drawing mode. #DefaultDrawingMode.
 */
class AbstractDrawingMode {
 public:
  virtual ~AbstractDrawingMode() = default;
  virtual void begin_sync() const = 0;
  virtual void image_sync(::Image *image, ::ImageUser *iuser) const = 0;
  virtual void draw_viewport() const = 0;
  virtual void draw_finish() const = 0;
};

}  // namespace blender::image_engine
