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
 * Copyright 2022, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_rect.h"

#include "GPU_batch.h"
#include "GPU_texture.h"

struct TextureInfo {
  /**
   * \brief Is the texture clipped.
   *
   * Resources of clipped textures are freed and ignored when performing partial updates.
   */
  bool visible : 1;

  /**
   * \brief does this texture need a full update.
   *
   * When set to false the texture can be updated using a partial update.
   */
  bool dirty : 1;

  /** \brief area of the texture in screen space. */
  rctf clipping_bounds;
  /** \brief uv area of the texture. */
  rctf uv_bounds;

  /**
   * \brief Batch to draw the associated texton the screen.
   *
   * contans a VBO with `pos` and 'uv'.
   * `pos` (2xF32) is relative to the origin of the space.
   * `uv` (2xF32) reflect the uv bounds.
   */
  GPUBatch *batch;

  /**
   * \brief GPU Texture for a partial region of the image editor.
   */
  GPUTexture *texture;

  ~TextureInfo()
  {
    if (batch != nullptr) {
      GPU_batch_discard(batch);
      batch = nullptr;
    }

    if (texture != nullptr) {
      GPU_texture_free(texture);
      texture = nullptr;
    }
  }
};
