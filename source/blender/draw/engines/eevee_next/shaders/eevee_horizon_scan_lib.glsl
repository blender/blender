/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Implementation of Horizon Based Global Illumination and Ambient Occlusion.
 *
 * This mostly follows the paper:
 * "Screen Space Indirect Lighting with Visibility Bitmask"
 * by Olivier Therrien, Yannick Levesque, Guillaume Gilet
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_fast_lib.glsl)

/**
 * Returns the bitmask for a given ordered pair of angle in [-pi/2..pi/2] range.
 * Clamps the inputs to the valid range.
 */
uint horizon_scan_angles_to_bitmask(vec2 theta)
{
  const int bitmask_len = 32;
  /* Algorithm 1, line 18. Re-ordered to make sure to clamp to the hemisphere range. */
  vec2 ratio = saturate(theta * M_1_PI + 0.5);
  uint a = uint(floor(float(bitmask_len) * ratio.x));
  /* The paper is wrong here. The additional half Pi is not needed. */
  uint b = uint(ceil(float(bitmask_len) * (ratio.y - ratio.x)));
  /* Algorithm 1, line 19. */
  return (((b < 32u) ? 1u << b : 0u) - 1u) << a;
}

float horizon_scan_bitmask_to_visibility_uniform(uint bitmask)
{
  const int bitmask_len = 32;
  /* Algorithm 1, line 26. */
  return float(bitCount(bitmask)) / float(bitmask_len);
}

/**
 * For a given visibility bitmask storing locally occluded sectors,
 * returns the uniform (non-cosine weighted) occlusion (visibility).
 */
float horizon_scan_bitmask_to_occlusion_uniform(uint bitmask)
{
  /* Occlusion is the opposite of visibility. */
  return 1.0 - horizon_scan_bitmask_to_visibility_uniform(bitmask);
}

/**
 * For a given visibility bitmask storing locally occluded sectors,
 * returns the cosine weighted occlusion (visibility).
 */
float horizon_scan_bitmask_to_occlusion_cosine(uint bitmask)
{
  const int bitmask_len = 32;
  /* This is not described in the paper. Another solution would be to change the sector
   * distribution in `horizon_scan_angles_to_bitmask()` but that requires more computation per
   * samples. The quality difference does not justify it currently. */

#if 0 /* Reference. */
  float visibility = 0.0;
  for (int bit = 0; bit < bitmask_len; bit++) {
    float angle = (((float(bit) + 0.5) / float(bitmask_len)) - 0.5) * M_PI;
    /* Integrating over the hemisphere. */
    if (((bitmask >> bit) & 1u) == 0u) {
      visibility += cos(angle) * M_PI_2 / float(bitmask_len);
    }
  }
  return visibility;
#else
  /* The precomputed weights are the accumulated weights from the reference loop for each of the
   * samples in the mask. The weight is distributed evenly for each sample inside a mask.
   * This is like a 4 piecewise linear approximation of the cosine lobe. */
  const vec4 weights = vec4(0.0095061, 0.0270951, 0.0405571, 0.0478421);
  const uvec4 masks = uvec4(0xF000000Fu, 0x0F0000F0u, 0x00F00F00u, 0x000FF000u);
  return saturate(1.0 - dot(vec4(bitCount(uvec4(bitmask) & masks)), weights));
#endif
}

float bsdf_eval(vec3 N, vec3 L, vec3 V)
{
  return dot(N, L);
}

/**
 * Projects the normal `N` onto a plane defined by `V` and `T`.
 * V, T, B forms an orthonormal basis around V.
 * Returns the angle of the normal projected normal with `V` and its length.
 */
void horizon_scan_projected_normal_to_plane_angle_and_length(
    vec3 N, vec3 V, vec3 T, vec3 B, out float N_proj_len, out float N_angle)
{
  /* Projected view normal onto the integration plane. */
  vec3 N_proj = normalize_and_get_length(N - B * dot(N, B), N_proj_len);

  float N_sin = dot(N_proj, T);
  float N_cos = dot(N_proj, V);
  /* Angle between normalized projected normal and view vector. */
  N_angle = sign(N_sin) * acos_fast(N_cos);
}
