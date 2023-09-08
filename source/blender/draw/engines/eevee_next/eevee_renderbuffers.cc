/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * A film is a full-screen buffer (usually at output extent)
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

void RenderBuffers::sync()
{
  const eViewLayerEEVEEPassType enabled_passes = inst_.film.enabled_passes_get();

  data.color_len = 0;
  data.value_len = 0;

  auto pass_index_get = [&](eViewLayerEEVEEPassType pass_type, int dependent_passes = 0) {
    if (enabled_passes & (pass_type | dependent_passes)) {
      return pass_storage_type(pass_type) == PASS_STORAGE_COLOR ? data.color_len++ :
                                                                  data.value_len++;
    }
    return -1;
  };

  data.normal_id = pass_index_get(EEVEE_RENDER_PASS_NORMAL, EEVEE_RENDER_PASS_AO);
  data.diffuse_light_id = pass_index_get(EEVEE_RENDER_PASS_DIFFUSE_LIGHT);
  data.diffuse_color_id = pass_index_get(EEVEE_RENDER_PASS_DIFFUSE_COLOR);
  data.specular_light_id = pass_index_get(EEVEE_RENDER_PASS_SPECULAR_LIGHT);
  data.specular_color_id = pass_index_get(EEVEE_RENDER_PASS_SPECULAR_COLOR);
  data.volume_light_id = pass_index_get(EEVEE_RENDER_PASS_VOLUME_LIGHT);
  data.emission_id = pass_index_get(EEVEE_RENDER_PASS_EMIT);
  data.environment_id = pass_index_get(EEVEE_RENDER_PASS_ENVIRONMENT);
  data.shadow_id = pass_index_get(EEVEE_RENDER_PASS_SHADOW);
  data.ambient_occlusion_id = pass_index_get(EEVEE_RENDER_PASS_AO);

  data.aovs = inst_.film.aovs_info;
}

void RenderBuffers::acquire(int2 extent)
{
  const eViewLayerEEVEEPassType enabled_passes = inst_.film.enabled_passes_get();

  auto pass_extent = [&](eViewLayerEEVEEPassType pass_bit) -> int2 {
    /* Use dummy texture for disabled passes. Allows correct bindings. */
    return (enabled_passes & pass_bit) ? extent : int2(1);
  };

  eGPUTextureFormat color_format = GPU_RGBA16F;
  eGPUTextureFormat float_format = GPU_R16F;
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

  /* Depth and combined are always needed. */
  depth_tx.ensure_2d(GPU_DEPTH24_STENCIL8, extent, usage | GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW);
  /* TODO(fclem): depth_tx should ideally be a texture from pool but we need stencil_view
   * which is currently unsupported by pool textures. */
  // depth_tx.acquire(extent, GPU_DEPTH24_STENCIL8);
  combined_tx.acquire(extent, color_format);

  eGPUTextureUsage usage_attachment_read_write = GPU_TEXTURE_USAGE_ATTACHMENT |
                                                 GPU_TEXTURE_USAGE_SHADER_READ |
                                                 GPU_TEXTURE_USAGE_SHADER_WRITE;

  /* TODO(fclem): Make vector pass allocation optional if no TAA or motion blur is needed. */
  vector_tx.acquire(extent,
                    vector_tx_format(),
                    usage_attachment_read_write | GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW);

  int color_len = data.color_len + data.aovs.color_len;
  int value_len = data.value_len + data.aovs.value_len;

  rp_color_tx.ensure_2d_array(color_format,
                              (color_len > 0) ? extent : int2(1),
                              math::max(1, color_len),
                              usage_attachment_read_write);
  rp_value_tx.ensure_2d_array(float_format,
                              (value_len > 0) ? extent : int2(1),
                              math::max(1, value_len),
                              usage_attachment_read_write);

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
      cryptomatte_format,
      GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE);
}

void RenderBuffers::release()
{
  /* TODO(fclem): depth_tx should ideally be a texture from pool but we need stencil_view
   * which is currently unsupported by pool textures. */
  // depth_tx.release();
  combined_tx.release();

  vector_tx.release();
  cryptomatte_tx.release();
}

eGPUTextureFormat RenderBuffers::vector_tx_format()
{
  const eViewLayerEEVEEPassType enabled_passes = inst_.film.enabled_passes_get();
  bool do_vector_render_pass = (enabled_passes & EEVEE_RENDER_PASS_VECTOR) ||
                               (inst_.motion_blur.postfx_enabled() && !inst_.is_viewport());

  /* Only RG16F when only doing only reprojection or motion blur. */
  return do_vector_render_pass ? GPU_RGBA16F : GPU_RG16F;
}

}  // namespace blender::eevee
