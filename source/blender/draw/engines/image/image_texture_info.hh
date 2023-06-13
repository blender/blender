/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_math_matrix.hh"
#include "BLI_rect.h"

#include "GPU_batch.h"
#include "GPU_texture.h"

namespace blender::draw::image_engine {

struct TextureInfo {
  /**
   * \brief does this texture need a full update.
   *
   * When set to false the texture can be updated using a partial update.
   */
  bool need_full_update : 1;

  /** \brief area of the texture in screen space. */
  rcti clipping_bounds;
  /** \brief uv area of the texture in screen space. */
  rctf clipping_uv_bounds;

  /* Which tile of the screen is used with this texture. Used to safely calculate the correct
   * offset of the textures. */
  int2 tile_id;

  /**
   * \brief Batch to draw the associated text on the screen.
   *
   * Contains a VBO with `pos` and `uv`.
   * `pos` (2xI32) is relative to the origin of the space.
   * `uv` (2xF32) reflect the uv bounds.
   */
  GPUBatch *batch = nullptr;

  /**
   * \brief GPU Texture for a partial region of the image editor.
   */
  GPUTexture *texture = nullptr;

  int2 last_texture_size = int2(0);

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

  /**
   * \brief return the offset of the texture with the area.
   *
   * A texture covers only a part of the area. The offset if the offset in screen coordinates
   * between the area and the part that the texture covers.
   */
  int2 offset() const
  {
    return int2(clipping_bounds.xmin, clipping_bounds.ymin);
  }

  void ensure_gpu_texture(int2 texture_size)
  {
    const bool is_allocated = texture != nullptr;
    const bool resolution_changed = assign_if_different(last_texture_size, texture_size);
    const bool should_be_freed = is_allocated && resolution_changed;
    const bool should_be_created = !is_allocated || resolution_changed;

    if (should_be_freed) {
      GPU_texture_free(texture);
      texture = nullptr;
    }

    if (should_be_created) {
      texture = DRW_texture_create_2d_ex(UNPACK2(texture_size),
                                         GPU_RGBA16F,
                                         GPU_TEXTURE_USAGE_GENERAL,
                                         static_cast<DRWTextureFlag>(0),
                                         nullptr);
    }
    need_full_update |= should_be_created;
  }
};

}  // namespace blender::draw::image_engine
