/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_volume_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_debug_surfels)

#include "eevee_sampling_lib.glsl"
#include "gpu_shader_debug_gradients_lib.glsl"

float3 debug_random_color(int v)
{
  float r = interleaved_gradient_noise(float2(v, 0), 0.0f, 0.0f);
  return hue_gradient(r);
}

void main()
{
  Surfel surfel = surfels_buf[surfel_index];

  float4 radiance_vis = float4(0.0f);
  radiance_vis += gl_FrontFacing ? surfel.radiance_direct.front : surfel.radiance_direct.back;
  radiance_vis += gl_FrontFacing ? surfel.radiance_indirect[1].front :
                                   surfel.radiance_indirect[1].back;

  switch (eDebugMode(debug_mode)) {
    default:
    case DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL:
      out_color = float4(pow(surfel.normal * 0.5f + 0.5f, float3(2.2f)), 0.0f);
      break;
    case DEBUG_IRRADIANCE_CACHE_SURFELS_CLUSTER:
      out_color = float4(pow(debug_random_color(surfel.cluster_id), float3(2.2f)), 0.0f);
      break;
    case DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE:
      out_color = float4(radiance_vis.rgb, 0.0f);
      break;
    case DEBUG_IRRADIANCE_CACHE_SURFELS_VISIBILITY:
      out_color = float4(radiance_vis.aaa, 0.0f);
      break;
  }

  /* Display surfels as circles. */
  if (distance(P, surfel.position) > debug_surfel_radius) {
    gpu_discard_fragment();
    return;
  }
}
