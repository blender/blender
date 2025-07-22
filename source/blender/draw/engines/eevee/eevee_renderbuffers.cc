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

#include "GPU_texture.hh"

#include "DRW_render.hh"

#include "eevee_film.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

void RenderBuffers::init()
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
  data.position_id = pass_index_get(EEVEE_RENDER_PASS_POSITION);
  data.diffuse_light_id = pass_index_get(EEVEE_RENDER_PASS_DIFFUSE_LIGHT);
  data.diffuse_color_id = pass_index_get(EEVEE_RENDER_PASS_DIFFUSE_COLOR);
  data.specular_light_id = pass_index_get(EEVEE_RENDER_PASS_SPECULAR_LIGHT);
  data.specular_color_id = pass_index_get(EEVEE_RENDER_PASS_SPECULAR_COLOR);
  data.volume_light_id = pass_index_get(EEVEE_RENDER_PASS_VOLUME_LIGHT);
  data.emission_id = pass_index_get(EEVEE_RENDER_PASS_EMIT);
  data.environment_id = pass_index_get(EEVEE_RENDER_PASS_ENVIRONMENT);
  data.shadow_id = pass_index_get(EEVEE_RENDER_PASS_SHADOW);
  data.ambient_occlusion_id = pass_index_get(EEVEE_RENDER_PASS_AO);
  data.transparent_id = pass_index_get(EEVEE_RENDER_PASS_TRANSPARENT);

  data.aovs = inst_.film.aovs_info;
}

void RenderBuffers::acquire(int2 extent)
{
  const eViewLayerEEVEEPassType enabled_passes = inst_.film.enabled_passes_get();

  extent_ = extent;

  auto pass_extent = [&](eViewLayerEEVEEPassType pass_bit) -> int2 {
    /* Use dummy texture for disabled passes. Allows correct bindings. */
    return (enabled_passes & pass_bit) ? extent : int2(1);
  };

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

  /* Depth and combined are always needed. */
  depth_tx.ensure_2d(gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8, extent, usage);
  /* TODO(fclem): depth_tx should ideally be a texture from pool but we need stencil_view
   * which is currently unsupported by pool textures. */
  // depth_tx.acquire(extent, gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8);
  combined_tx.acquire(extent, color_format);

  eGPUTextureUsage usage_attachment_read_write = GPU_TEXTURE_USAGE_ATTACHMENT |
                                                 GPU_TEXTURE_USAGE_SHADER_READ |
                                                 GPU_TEXTURE_USAGE_SHADER_WRITE;

  /* TODO(fclem): Make vector pass allocation optional if no TAA or motion blur is needed. */
  vector_tx.acquire(extent, vector_tx_format(), usage_attachment_read_write);

  const bool do_motion_vectors_swizzle = vector_tx_format() == gpu::TextureFormat::SFLOAT_16_16;
  if (do_motion_vectors_swizzle) {
    /* Change texture swizzling to avoid complexity in shaders. */
    GPU_texture_swizzle_set(vector_tx, "rgrg");
  }

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

  const gpu::TextureFormat cryptomatte_format = gpu::TextureFormat::SFLOAT_32_32_32_32;
  cryptomatte_tx.acquire(pass_extent(EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT |
                                     EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET |
                                     EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL),
                         cryptomatte_format,
                         GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE);
}

void RenderBuffers::release()
{
  /* TODO(fclem): depth_tx should ideally be a texture from pool but we need stencil_view
   * which is currently unsupported by pool textures. */
  // depth_tx.release();
  combined_tx.release();

  const bool do_motion_vectors_swizzle = vector_tx_format() == gpu::TextureFormat::SFLOAT_16_16;
  if (do_motion_vectors_swizzle) {
    /* Reset swizzle since this texture might be reused in other places. */
    GPU_texture_swizzle_set(vector_tx, "rgba");
  }
  vector_tx.release();

  cryptomatte_tx.release();
}

gpu::TextureFormat RenderBuffers::vector_tx_format()
{
  const eViewLayerEEVEEPassType enabled_passes = inst_.film.enabled_passes_get();
  bool do_full_vector_render_pass = ((enabled_passes & EEVEE_RENDER_PASS_VECTOR) ||
                                     inst_.motion_blur.postfx_enabled()) &&
                                    !inst_.is_viewport();

  /* Only RG16F (`motion.prev`) for the viewport. */
  return do_full_vector_render_pass ? gpu::TextureFormat::SFLOAT_16_16_16_16 :
                                      gpu::TextureFormat::SFLOAT_16_16;
}

}  // namespace blender::eevee
