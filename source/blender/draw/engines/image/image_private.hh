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
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include <optional>

#include "BKE_image.h"

#include "image_instance_data.hh"
#include "image_texture_info.hh"

/* Forward declarations */
extern "C" {
struct GPUTexture;
struct ImBuf;
struct Image;
}

/* *********** LISTS *********** */

namespace blender::draw::image_engine {

struct IMAGE_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  IMAGE_InstanceData *instance_data;
};

/* Shader parameters. */
#define IMAGE_DRAW_FLAG_SHOW_ALPHA (1 << 0)
#define IMAGE_DRAW_FLAG_APPLY_ALPHA (1 << 1)
#define IMAGE_DRAW_FLAG_SHUFFLING (1 << 2)
#define IMAGE_DRAW_FLAG_DEPTH (1 << 3)
/** Flag to disable depth testing (used for node editor back drop drawing).*/
#define IMAGE_DRAW_FLAG_DEPTH_ALWAYS (1 << 4)

/**
 * Abstract class for a drawing mode of the image engine.
 *
 * The drawing mode decides how to draw the image on the screen. Each way how to draw would have
 * its own subclass. For now there is only a single drawing mode. #DefaultDrawingMode.
 **/
class AbstractDrawingMode {
 public:
  virtual ~AbstractDrawingMode() = default;
  virtual void cache_init(IMAGE_Data *vedata) const = 0;
  virtual void cache_image(IMAGE_Data *vedata, Image *image, ImageUser *iuser) const = 0;
  virtual void draw_scene(IMAGE_Data *vedata) const = 0;
  virtual void draw_finish(IMAGE_Data *vedata) const = 0;
};

/* image_shader.c */
GPUShader *IMAGE_shader_image_get();
GPUShader *IMAGE_shader_depth_get();
void IMAGE_shader_free();

}  // namespace blender::draw::image_engine
