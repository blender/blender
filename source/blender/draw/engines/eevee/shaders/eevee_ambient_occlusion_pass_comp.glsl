/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_ambient_occlusion_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_ambient_occlusion_pass)

#include "eevee_horizon_scan_eval_lib.glsl"
#include "eevee_utility_tx_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 extent = imageSize(in_normal_img).xy;
  if (any(greaterThanEqual(texel, extent))) {
    return;
  }

  float2 uv = (float2(texel) + float2(0.5f)) / float2(extent);
  float depth = texelFetch(hiz_tx, texel, 0).r;

  if (depth == 1.0f) {
    /* Do not trace for background */
    imageStoreFast(out_ao_img, int3(texel, out_ao_img_layer_index), float4(0.0f));
    return;
  }

  float3 vP = drw_point_screen_to_view(float3(uv, depth));
  float3 N = imageLoad(in_normal_img, int3(texel, in_normal_img_layer_index)).xyz;
  float3 vN = drw_normal_world_to_view(N);

  auto &lut_tx = sampler_get(eevee_utility_texture, utility_tx);
  float4 noise = utility_tx_fetch(lut_tx, float2(texel), UTIL_BLUE_NOISE_LAYER);
  noise = fract(noise + sampling_rng_3D_get(SAMPLING_AO_U).xyzx);

  HorizonScanResult scan = horizon_scan_eval(vP,
                                             vN,
                                             noise,
                                             uniform_buf.ao.pixel_size,
                                             uniform_buf.ao.distance,
                                             uniform_buf.ao.thickness_near,
                                             uniform_buf.ao.thickness_far,
                                             uniform_buf.ao.angle_bias,
                                             ao_slice_count,
                                             ao_step_count,
                                             false,
                                             true);

  imageStoreFast(out_ao_img, int3(texel, out_ao_img_layer_index), float4(saturate(scan.result)));
}
