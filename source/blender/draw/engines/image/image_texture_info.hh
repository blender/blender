/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_math_vec_types.hh"
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

  /**
   * \brief Update the region bounds from the uv bounds by applying the given transform matrix.
   */
  void calc_region_bounds_from_uv_bounds(const float4x4 &uv_to_region)
  {
    float3 bottom_left_uv = float3(clipping_uv_bounds.xmin, clipping_uv_bounds.ymin, 0.0f);
    float3 top_right_uv = float3(clipping_uv_bounds.xmax, clipping_uv_bounds.ymax, 0.0f);
    float3 bottom_left_region = uv_to_region * bottom_left_uv;
    float3 top_right_region = uv_to_region * top_right_uv;
    BLI_rctf_init(&clipping_bounds,
                  bottom_left_region.x,
                  top_right_region.x,
                  bottom_left_region.y,
                  top_right_region.y);
  }
};

}  // namespace blender::draw::image_engine
