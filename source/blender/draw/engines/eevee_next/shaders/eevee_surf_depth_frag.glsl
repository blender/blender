/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Depth shader that can stochastically discard transparent pixel.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_transparency_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  vec4 out_color;
  out_color.rgb = g_emission;
  out_color.a = saturate(1.0 - average(g_transmittance));

  /* Reset for the next closure tree. */
  closure_weights_reset();

  return out_color;
}

void main()
{
#ifdef MAT_TRANSPARENT
  init_globals();

  nodetree_surface();

#  ifdef MAT_FORWARD
  /* Pre-pass only allows fully opaque areas to cut through all transparent layers. */
  float threshold = 0.0;
#  else
  float noise_offset = sampling_rng_1D_get(SAMPLING_TRANSPARENCY);
  float threshold = transparency_hashed_alpha_threshold(
      uniform_buf.pipeline.alpha_hash_scale, noise_offset, g_data.P);
#  endif

  float transparency = average(g_transmittance);
  if (transparency > threshold) {
    discard;
    return;
  }
#endif

#ifdef MAT_CLIP_PLANE
  /* Do not use hardware clip planes as they modify the rasterization (some GPUs add vertices).
   * This would in turn create a discrepancy between the pre-pass depth and the G-buffer depth
   * which exhibits missing pixels data. */
  if (clip_interp.clip_distance > 0.0) {
    discard;
    return;
  }
#endif

#ifdef MAT_VELOCITY
  out_velocity = velocity_surface(interp.P + motion.prev, interp.P, interp.P + motion.next);
  out_velocity = velocity_pack(out_velocity);
#endif
}
