/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)

/* Similar to https://atyuwen.github.io/posts/normal-reconstruction/.
 * This samples the depth buffer 4 time for each direction to get the most correct
 * implicit normal reconstruction out of the depth buffer. */
vec3 view_position_derivative_from_depth(sampler2D scene_depth_tx,
                                         ivec2 extent,
                                         vec2 uv,
                                         ivec2 texel,
                                         ivec2 offset,
                                         vec3 vP,
                                         float depth_center)
{
  vec4 H;
  H.x = texelFetch(scene_depth_tx, texel - offset * 2, 0).r;
  H.y = texelFetch(scene_depth_tx, texel - offset, 0).r;
  H.z = texelFetch(scene_depth_tx, texel + offset, 0).r;
  H.w = texelFetch(scene_depth_tx, texel + offset * 2, 0).r;

  vec2 uv_offset = vec2(offset) / vec2(extent);
  vec2 uv1 = uv - uv_offset * 2.0;
  vec2 uv2 = uv - uv_offset;
  vec2 uv3 = uv + uv_offset;
  vec2 uv4 = uv + uv_offset * 2.0;

  /* Fix issue with depth precision. Take even larger diff. */
  vec4 diff = abs(vec4(depth_center, H.yzw) - H.x);
  if (reduce_max(diff) < 2.4e-7 && all(lessThan(diff.xyz, diff.www))) {
    vec3 P1 = drw_point_screen_to_view(vec3(uv1, H.x));
    vec3 P3 = drw_point_screen_to_view(vec3(uv3, H.w));
    return 0.25 * (P3 - P1);
  }
  /* Simplified (H.xw + 2.0 * (H.yz - H.xw)) - depth_center */
  vec2 deltas = abs((2.0 * H.yz - H.xw) - depth_center);
  if (deltas.x < deltas.y) {
    return vP - drw_point_screen_to_view(vec3(uv2, H.y));
  }
  return drw_point_screen_to_view(vec3(uv3, H.z)) - vP;
}

struct SurfaceReconstructResult {
  /* View position. */
  vec3 vP;
  /* View geometric normal. */
  vec3 vNg;
  /* Screen depth [0..1]. Corresponding to the depth buffer value. */
  float depth;
  /* True if the pixel has background depth. */
  bool is_background;
};

/**
 * Reconstruct surface information from the depth buffer.
 * Use adjacent pixel info to reconstruct normals.

 * \a extent is the valid region of depth_tx.
 * \a texel is the pixel coordinate [0..extent-1] to reconstruct.
 */
SurfaceReconstructResult view_reconstruct_from_depth(sampler2D scene_depth_tx,
                                                     ivec2 extent,
                                                     ivec2 texel)
{
  SurfaceReconstructResult result;
  result.depth = texelFetch(scene_depth_tx, texel, 0).r;
  result.is_background = (result.depth == 1.0);
  vec2 uv = (vec2(texel) + vec2(0.5)) / vec2(extent);
  result.vP = drw_point_screen_to_view(vec3(uv, result.depth));
  if (result.is_background) {
    result.vNg = drw_view_incident_vector(result.vP);
    return result;
  }
  vec3 dPdx = view_position_derivative_from_depth(
      scene_depth_tx, extent, uv, texel, ivec2(1, 0), result.vP, result.depth);
  vec3 dPdy = view_position_derivative_from_depth(
      scene_depth_tx, extent, uv, texel, ivec2(0, 1), result.vP, result.depth);
  result.vNg = safe_normalize(cross(dPdx, dPdy));
  return result;
}
