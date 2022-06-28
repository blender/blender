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

void RenderBuffers::sync()
{
  depth_tx.sync();
  combined_tx.sync();

  normal_tx.sync();
  vector_tx.sync();
  diffuse_light_tx.sync();
  diffuse_color_tx.sync();
  specular_light_tx.sync();
  specular_color_tx.sync();
  volume_light_tx.sync();
  emission_tx.sync();
  environment_tx.sync();
  shadow_tx.sync();
  ambient_occlusion_tx.sync();
}

void RenderBuffers::acquire(int2 extent, void *owner)
{
  auto pass_extent = [&](eViewLayerEEVEEPassType pass_bit) -> int2 {
    /* Use dummy texture for disabled passes. Allows correct bindings. */
    return (inst_.film.enabled_passes_get() & pass_bit) ? extent : int2(1);
  };

  eGPUTextureFormat color_format = GPU_RGBA16F;
  eGPUTextureFormat float_format = GPU_R16F;

  /* Depth and combined are always needed. */
  depth_tx.acquire(extent, GPU_DEPTH24_STENCIL8, owner);
  combined_tx.acquire(extent, color_format, owner);

  normal_tx.acquire(pass_extent(EEVEE_RENDER_PASS_NORMAL), color_format, owner);
  vector_tx.acquire(pass_extent(EEVEE_RENDER_PASS_VECTOR), color_format, owner);
  diffuse_light_tx.acquire(pass_extent(EEVEE_RENDER_PASS_DIFFUSE_LIGHT), color_format, owner);
  diffuse_color_tx.acquire(pass_extent(EEVEE_RENDER_PASS_DIFFUSE_COLOR), color_format, owner);
  specular_light_tx.acquire(pass_extent(EEVEE_RENDER_PASS_SPECULAR_LIGHT), color_format, owner);
  specular_color_tx.acquire(pass_extent(EEVEE_RENDER_PASS_SPECULAR_COLOR), color_format, owner);
  volume_light_tx.acquire(pass_extent(EEVEE_RENDER_PASS_VOLUME_LIGHT), color_format, owner);
  emission_tx.acquire(pass_extent(EEVEE_RENDER_PASS_EMIT), color_format, owner);
  environment_tx.acquire(pass_extent(EEVEE_RENDER_PASS_ENVIRONMENT), color_format, owner);
  shadow_tx.acquire(pass_extent(EEVEE_RENDER_PASS_SHADOW), float_format, owner);
  ambient_occlusion_tx.acquire(pass_extent(EEVEE_RENDER_PASS_AO), float_format, owner);

  const AOVsInfoData &aovs = inst_.film.aovs_info;
  aov_color_tx.ensure_2d_array(
      color_format, (aovs.color_len > 0) ? extent : int2(1), max_ii(1, aovs.color_len));
  aov_value_tx.ensure_2d_array(
      float_format, (aovs.value_len > 0) ? extent : int2(1), max_ii(1, aovs.value_len));
}

void RenderBuffers::release()
{
  depth_tx.release();
  combined_tx.release();

  normal_tx.release();
  vector_tx.release();
  diffuse_light_tx.release();
  diffuse_color_tx.release();
  specular_light_tx.release();
  specular_color_tx.release();
  volume_light_tx.release();
  emission_tx.release();
  environment_tx.release();
  shadow_tx.release();
  ambient_occlusion_tx.release();
}

}  // namespace blender::eevee
