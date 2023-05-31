/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_rand.hh"
#include "eevee_instance.hh"

#include "eevee_irradiance_cache.hh"

namespace blender::eevee {

void IrradianceCache::generate_random_surfels()
{
  const int surfels_len = 256;
  debug_surfels.resize(surfels_len);

  RandomNumberGenerator rng;
  rng.seed(0);

  for (DebugSurfel &surfel : debug_surfels) {
    float3 random = rng.get_unit_float3();
    surfel.position = random * 3.0f;
    surfel.normal = random;
    surfel.color = float4(rng.get_float(), rng.get_float(), rng.get_float(), 1.0f);
  }

  debug_surfels.push_update();
}

void IrradianceCache::init()
{
  if (debug_surfels_sh_ == nullptr) {
    debug_surfels_sh_ = inst_.shaders.static_shader_get(DEBUG_SURFELS);
  }

  /* TODO: Remove this. */
  generate_random_surfels();
}

void IrradianceCache::sync()
{
  debug_pass_sync();
}

void IrradianceCache::debug_pass_sync()
{
  if (inst_.debug_mode == eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS) {
    debug_surfels_ps_.init();
    debug_surfels_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                DRW_STATE_DEPTH_LESS_EQUAL);
    debug_surfels_ps_.shader_set(debug_surfels_sh_);
    debug_surfels_ps_.bind_ssbo("surfels_buf", debug_surfels);
    debug_surfels_ps_.push_constant("surfel_radius", 0.25f);
    debug_surfels_ps_.draw_procedural(GPU_PRIM_TRI_STRIP, debug_surfels.size(), 4);
  }
}

void IrradianceCache::debug_draw(View &view, GPUFrameBuffer *view_fb)
{
  if (inst_.debug_mode == eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS) {
    inst_.info = "Debug Mode: Irradiance Cache Surfels";
    GPU_framebuffer_bind(view_fb);
    inst_.manager->submit(debug_surfels_ps_, view);
  }
}

}  // namespace blender::eevee
