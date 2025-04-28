/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* Similar to https://atyuwen.github.io/posts/normal-reconstruction/.
 * This samples the depth buffer 4 time for each direction to get the most correct
 * implicit normal reconstruction out of the depth buffer. */
float3 view_position_derivative_from_depth(sampler2D scene_depth_tx,
                                           int2 extent,
                                           float2 uv,
                                           int2 texel,
                                           int2 offset,
                                           float3 vP,
                                           float depth_center)
{
  float4 H;
  H.x = texelFetch(scene_depth_tx, texel - offset * 2, 0).r;
  H.y = texelFetch(scene_depth_tx, texel - offset, 0).r;
  H.z = texelFetch(scene_depth_tx, texel + offset, 0).r;
  H.w = texelFetch(scene_depth_tx, texel + offset * 2, 0).r;

  float2 uv_offset = float2(offset) / float2(extent);
  float2 uv1 = uv - uv_offset * 2.0f;
  float2 uv2 = uv - uv_offset;
  float2 uv3 = uv + uv_offset;
  float2 uv4 = uv + uv_offset * 2.0f;

  /* Fix issue with depth precision. Take even larger diff. */
  float4 diff = abs(float4(depth_center, H.yzw) - H.x);
  if (reduce_max(diff) < 2.4e-7f && all(lessThan(diff.xyz, diff.www))) {
    float3 P1 = drw_point_screen_to_view(float3(uv1, H.x));
    float3 P3 = drw_point_screen_to_view(float3(uv3, H.w));
    return 0.25f * (P3 - P1);
  }
  /* Simplified (H.xw + 2.0f * (H.yz - H.xw)) - depth_center */
  float2 deltas = abs((2.0f * H.yz - H.xw) - depth_center);
  if (deltas.x < deltas.y) {
    return vP - drw_point_screen_to_view(float3(uv2, H.y));
  }
  return drw_point_screen_to_view(float3(uv3, H.z)) - vP;
}

struct SurfaceReconstructResult {
  /* View position. */
  float3 vP;
  /* View geometric normal. */
  float3 vNg;
  /* Screen depth [0..1]. Corresponding to the depth buffer value. */
  float depth;
  /* True if the pixel has background depth. */
  bool is_background;
};

/**
 * Reconstruct surface information from the depth buffer.
 * Use adjacent pixel info to reconstruct normals.
 *
 * \a extent is the valid region of depth_tx.
 * \a texel is the pixel coordinate [0..extent-1] to reconstruct.
 */
SurfaceReconstructResult view_reconstruct_from_depth(sampler2D scene_depth_tx,
                                                     int2 extent,
                                                     int2 texel)
{
  SurfaceReconstructResult result;
  result.depth = texelFetch(scene_depth_tx, texel, 0).r;
  result.is_background = (result.depth == 1.0f);
  float2 uv = (float2(texel) + float2(0.5f)) / float2(extent);
  result.vP = drw_point_screen_to_view(float3(uv, result.depth));
  if (result.is_background) {
    result.vNg = drw_view_incident_vector(result.vP);
    return result;
  }
  float3 dPdx = view_position_derivative_from_depth(
      scene_depth_tx, extent, uv, texel, int2(1, 0), result.vP, result.depth);
  float3 dPdy = view_position_derivative_from_depth(
      scene_depth_tx, extent, uv, texel, int2(0, 1), result.vP, result.depth);
  result.vNg = safe_normalize(cross(dPdx, dPdy));
  return result;
}
