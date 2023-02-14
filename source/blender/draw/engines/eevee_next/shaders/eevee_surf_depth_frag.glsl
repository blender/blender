
/**
 * Depth shader that can stochastically discard transparent pixel.
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
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
  out_color.a = saturate(1.0 - avg(g_transmittance));

  /* Reset for the next closure tree. */
  closure_weights_reset();

  return out_color;
}

void main()
{
#ifdef MAT_TRANSPARENT
  init_globals();

  nodetree_surface();

  float noise_offset = sampling_rng_1D_get(SAMPLING_TRANSPARENCY);
  float random_threshold = transparency_hashed_alpha_threshold(1.0, noise_offset, g_data.P);

  float transparency = avg(g_transmittance);
  if (transparency > random_threshold) {
    discard;
    return;
  }
#endif

#ifdef MAT_VELOCITY
  out_velocity = velocity_surface(interp.P + motion.prev, interp.P, interp.P + motion.next);
  out_velocity = velocity_pack(out_velocity);
#endif
}
