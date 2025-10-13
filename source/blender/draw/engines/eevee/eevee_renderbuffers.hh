/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Render buffers are textures that are filled during a view rendering.
 * Their content is then added to the accumulation buffers of the film class.
 * They are short lived and can be reused when doing multi view rendering.
 */

#pragma once

#include "DRW_render.hh"

#include "eevee_renderbuffers_shared.hh"

namespace blender::eevee {

class Instance;

class RenderBuffers {
 public:
  RenderBuffersInfoData &data;

  static constexpr gpu::TextureFormat color_format = gpu::TextureFormat::SFLOAT_16_16_16_16;
  static constexpr gpu::TextureFormat float_format = gpu::TextureFormat::SFLOAT_16;

  Texture depth_tx;
  TextureFromPool combined_tx;

  // TextureFromPool mist_tx; /* Derived from depth_tx during accumulation. */
  TextureFromPool vector_tx;
  TextureFromPool cryptomatte_tx;
  /* TODO(fclem): Use texture from pool once they support texture array. */
  Texture rp_color_tx;
  Texture rp_value_tx;

 private:
  Instance &inst_;

  int2 extent_;

 public:
  RenderBuffers(Instance &inst, RenderBuffersInfoData &data) : data(data), inst_(inst) {};

  /** WARNING: RenderBuffers and Film use different storage types for AO and Shadow. */
  static ePassStorageType pass_storage_type(eViewLayerEEVEEPassType pass_type)
  {
    switch (pass_type) {
      case EEVEE_RENDER_PASS_DEPTH:
      case EEVEE_RENDER_PASS_MIST:
      case EEVEE_RENDER_PASS_SHADOW:
      case EEVEE_RENDER_PASS_AO:
        return PASS_STORAGE_VALUE;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT:
      case EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET:
      case EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL:
        return PASS_STORAGE_CRYPTOMATTE;
      default:
        return PASS_STORAGE_COLOR;
    }
  }

  void init();

  /* Acquires (also ensures) the render buffer before rendering to them. */
  void acquire(int2 extent);
  void release();

  /* Return the size of the allocated render buffers. Undefined if called before `acquire()`. */
  int2 extent_get() const
  {
    return extent_;
  }

  gpu::TextureFormat vector_tx_format();
};

}  // namespace blender::eevee
