/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using captured Gbuffer data.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  ClosureLightStack stack;
  stack.cl[0].N = gbuf.has_diffuse ? gbuf.diffuse.N : gbuf.reflection.N;
  stack.cl[0].ltc_mat = LTC_LAMBERT_MAT;
  stack.cl[0].type = LIGHT_DIFFUSE;

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  vec3 Ng = stack.cl[0].N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  /* Direct light. */
  light_eval(stack, P, Ng, V, vPz, gbuf.thickness);
  /* Indirect light. */
  /* Can only load irradiance to avoid dependency loop with the reflection probe. */
  SphericalHarmonicL1 sh = lightprobe_irradiance_sample(P, V, Ng);

  vec3 radiance = stack.cl[0].light_shadowed + spherical_harmonics_evaluate_lambert(Ng, sh);

  vec3 albedo = gbuf.diffuse.color + gbuf.reflection.color + gbuf.refraction.color;

  out_radiance = vec4(radiance * albedo, 0.0);
}
