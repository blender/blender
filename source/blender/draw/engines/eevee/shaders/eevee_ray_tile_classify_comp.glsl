/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This pass load Gbuffer data and output a mask of tiles to process.
 * This mask is then processed by the compaction phase.
 */

#include "infos/eevee_tracing_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_ray_tile_classify)

#include "eevee_closure_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

shared uint tile_contains_ray_tracing[GBUFFER_LAYER_MAX];
shared uint tile_contains_horizon_scan;

/* Returns a blend factor between different tracing method. */
float ray_roughness_factor(RayTraceData raytrace, float roughness)
{
  return saturate(roughness * raytrace.roughness_mask_scale - raytrace.roughness_mask_bias);
}

void main()
{
  if (gl_LocalInvocationIndex == 0u) {
    /* Init shared variables. */
    tile_contains_horizon_scan = 0;
    for (int i = 0; i < GBUFFER_LAYER_MAX; i++) {
      tile_contains_ray_tracing[i] = 0;
    }
  }

  barrier();

  int2 texel = int2(gl_GlobalInvocationID.xy);

  bool valid_texel = in_texture_range(texel, gbuf_header_tx);

  if (valid_texel) {
    gbuffer::Header header = gbuffer::read_header(texel);

    for (uchar i = 0; i < GBUFFER_LAYER_MAX; i++) {
      ClosureUndetermined cl = gbuffer::read_bin(header, texel, i);
      if (cl.type == CLOSURE_NONE_ID) {
        continue;
      }
      float roughness = closure_apparent_roughness_get(cl);
      float ray_roughness_fac = ray_roughness_factor(uniform_buf.raytrace, roughness);

      /* We don't care about race condition here. */
      if (ray_roughness_fac > 0.0f) {
        tile_contains_horizon_scan = 1;
      }
      if (ray_roughness_fac < 1.0f) {
        tile_contains_ray_tracing[i] = 1;
      }
    }
  }

  barrier();

  if (gl_LocalInvocationIndex == 0u) {
    int2 denoise_tile_co = int2(gl_WorkGroupID.xy);
    int2 tracing_tile_co = denoise_tile_co / uniform_buf.raytrace.resolution_scale;

    for (int i = 0; i < GBUFFER_LAYER_MAX; i++) {
      if (tile_contains_ray_tracing[i] > 0) {
        imageStoreFast(tile_raytrace_denoise_img, int3(denoise_tile_co, i), uint4(1));
        imageStoreFast(tile_raytrace_tracing_img, int3(tracing_tile_co, i), uint4(1));
      }
    }

    if (tile_contains_horizon_scan > 0) {
      int2 tracing_tile_co = denoise_tile_co / uniform_buf.raytrace.horizon_resolution_scale;
      imageStoreFast(tile_horizon_denoise_img, int3(denoise_tile_co, 0), uint4(1));
      imageStoreFast(tile_horizon_tracing_img, int3(tracing_tile_co, 0), uint4(1));
    }
  }
}
