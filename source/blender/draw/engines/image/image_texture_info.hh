/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

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
  /** \brief uv area of the texture in screen space. */
  rctf clipping_uv_bounds;

  /**
   * \brief Batch to draw the associated text on the screen.
   *
   * Contains a VBO with `pos` and `uv`.
   * `pos` (2xF32) is relative to the origin of the space.
   * `uv` (2xF32) reflect the uv bounds.
   */
  GPUBatch *batch;

  /**
   * \brief GPU Texture for a partial region of the image editor.
   */
  GPUTexture *texture;

  float2 last_viewport_size = float2(0.0f, 0.0f);

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
