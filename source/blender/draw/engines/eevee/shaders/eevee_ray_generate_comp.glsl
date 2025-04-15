/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Generate Ray direction along with other data that are then used
 * by the next pass to trace the rays.
 */

#include "infos/eevee_tracing_info.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_ray_generate)

#include "eevee_gbuffer_lib.glsl"
#include "eevee_ray_generate_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

void main()
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  int2 texel = int2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  int2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                       uniform_buf.raytrace.resolution_bias;

  uint gbuf_header = texelFetch(gbuf_header_tx, int3(texel_fullres, 0), 0).r;
  ClosureUndetermined closure = gbuffer_read_bin(
      gbuf_header, gbuf_closure_tx, gbuf_normal_tx, texel_fullres, closure_index);

  if (closure.type == CLOSURE_NONE_ID) {
    imageStore(out_ray_data_img, texel, float4(0.0f));
    return;
  }

  float2 uv = (float2(texel_fullres) + 0.5f) / float2(textureSize(gbuf_header_tx, 0).xy);
  float3 P = drw_point_screen_to_world(float3(uv, 0.5f));
  float3 V = drw_world_incident_vector(P);
  float2 noise = utility_tx_fetch(utility_tx, float2(texel), UTIL_BLUE_NOISE_LAYER).rg;
  noise = fract(noise + sampling_rng_2D_get(SAMPLING_RAYTRACE_U));

  float thickness = gbuffer_read_thickness(gbuf_header, gbuf_normal_tx, texel_fullres);

  BsdfSample samp = ray_generate_direction(noise.xy, closure, V, thickness);

  /* Store inverse pdf to speedup denoising.
   * Limit to the smallest non-0 value that the format can encode.
   * Strangely it does not correspond to the IEEE spec. */
  float inv_pdf = (samp.pdf == 0.0f) ? 0.0f : max(6e-8f, 1.0f / samp.pdf);
  imageStoreFast(out_ray_data_img, texel, float4(samp.direction, inv_pdf));
}
