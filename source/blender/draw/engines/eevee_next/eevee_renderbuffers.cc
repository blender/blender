/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * A film is a fullscreen buffer (usually at output extent)
 * that will be able to accumulate sample in any distorted camera_type
 * using a pixel filter.
 *
 * Input needs to be jittered so that the filter converges to the right result.
 */

#include "BLI_rect.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "DRW_render.h"

#include "eevee_film.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

void RenderBuffers::acquire(int2 extent)
{
  const eViewLayerEEVEEPassType enabled_passes = inst_.film.enabled_passes_get();

  auto pass_extent = [&](eViewLayerEEVEEPassType pass_bit) -> int2 {
    /* Use dummy texture for disabled passes. Allows correct bindings. */
    return (enabled_passes & pass_bit) ? extent : int2(1);
  };

  eGPUTextureFormat color_format = GPU_RGBA16F;
  eGPUTextureFormat float_format = GPU_R16F;

  /* Depth and combined are always needed. */
  depth_tx.acquire(extent, GPU_DEPTH24_STENCIL8);
  combined_tx.acquire(extent, color_format);

  bool do_vector_render_pass = (enabled_passes & EEVEE_RENDER_PASS_VECTOR) ||
                               (inst_.motion_blur.postfx_enabled() && !inst_.is_viewport());
  uint32_t max_light_color_layer = max_ii(enabled_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT ?
                                              (int)RENDER_PASS_LAYER_DIFFUSE_LIGHT :
                                              -1,
                                          enabled_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT ?
                                              (int)RENDER_PASS_LAYER_SPECULAR_LIGHT :
                                              -1) +
                                   1;
  /* Only RG16F when only doing only reprojection or motion blur. */
  eGPUTextureFormat vector_format = do_vector_render_pass ? GPU_RGBA16F : GPU_RG16F;
  /* TODO(fclem): Make vector pass allocation optional if no TAA or motion blur is needed. */
  vector_tx.acquire(extent, vector_format);

  normal_tx.acquire(pass_extent(EEVEE_RENDER_PASS_NORMAL), color_format);
  diffuse_color_tx.acquire(pass_extent(EEVEE_RENDER_PASS_DIFFUSE_COLOR), color_format);
  specular_color_tx.acquire(pass_extent(EEVEE_RENDER_PASS_SPECULAR_COLOR), color_format);
  volume_light_tx.acquire(pass_extent(EEVEE_RENDER_PASS_VOLUME_LIGHT), color_format);
  emission_tx.acquire(pass_extent(EEVEE_RENDER_PASS_EMIT), color_format);
  environment_tx.acquire(pass_extent(EEVEE_RENDER_PASS_ENVIRONMENT), color_format);
  shadow_tx.acquire(pass_extent(EEVEE_RENDER_PASS_SHADOW), float_format);
  ambient_occlusion_tx.acquire(pass_extent(EEVEE_RENDER_PASS_AO), float_format);

  light_tx.ensure_2d_array(color_format,
                           max_light_color_layer > 0 ? extent : int2(1),
                           max_ii(1, max_light_color_layer));

  const AOVsInfoData &aovs = inst_.film.aovs_info;
  aov_color_tx.ensure_2d_array(
      color_format, (aovs.color_len > 0) ? extent : int2(1), max_ii(1, aovs.color_len));
  aov_value_tx.ensure_2d_array(
      float_format, (aovs.value_len > 0) ? extent : int2(1), max_ii(1, aovs.value_len));

  eGPUTextureFormat cryptomatte_format = GPU_R32F;
  const int cryptomatte_layer_len = inst_.film.cryptomatte_layer_max_get();
  if (cryptomatte_layer_len == 2) {
    cryptomatte_format = GPU_RG32F;
  }
  else if (cryptomatte_layer_len == 3) {
    cryptomatte_format = GPU_RGBA32F;
  }
  cryptomatte_tx.acquire(
      pass_extent(static_cast<eViewLayerEEVEEPassType>(EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT |
                                                       EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET |
                                                       EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL)),
      cryptomatte_format);
}

void RenderBuffers::release()
{
  depth_tx.release();
  combined_tx.release();

  normal_tx.release();
  vector_tx.release();
  diffuse_color_tx.release();
  specular_color_tx.release();
  volume_light_tx.release();
  emission_tx.release();
  environment_tx.release();
  shadow_tx.release();
  ambient_occlusion_tx.release();
  cryptomatte_tx.release();
}

}  // namespace blender::eevee
