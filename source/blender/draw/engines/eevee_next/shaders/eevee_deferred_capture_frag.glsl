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

  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  if (gbuf.closure_count == 0) {
    out_radiance = vec4(0.0);
    return;
  }

  ClosureLightStack stack;
  stack.cl[0].N = gbuf.surface_N;
  stack.cl[0].ltc_mat = LTC_LAMBERT_MAT;
  stack.cl[0].type = LIGHT_DIFFUSE;

  stack.cl[1].N = -gbuf.surface_N;
  stack.cl[1].ltc_mat = LTC_LAMBERT_MAT;
  stack.cl[1].type = LIGHT_DIFFUSE;

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  vec3 Ng = gbuf.surface_N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  /* Direct light. */
  light_eval(stack, P, Ng, V, vPz, gbuf.thickness);

  /* Indirect light. */
  /* Can only load irradiance to avoid dependency loop with the reflection probe. */
  SphericalHarmonicL1 sh = lightprobe_irradiance_sample(P, V, Ng);

  vec3 radiance_front = stack.cl[0].light_shadowed + spherical_harmonics_evaluate_lambert(Ng, sh);
  vec3 radiance_back = stack.cl[1].light_shadowed + spherical_harmonics_evaluate_lambert(-Ng, sh);

  vec3 albedo_front = vec3(0.0);
  vec3 albedo_back = vec3(0.0);

  for (int i = 0; i < GBUFFER_LAYER_MAX && i < gbuf.closure_count; i++) {
    ClosureUndetermined cl = gbuffer_closure_get(gbuf, i);
    switch (cl.type) {
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
        albedo_front += cl.color;
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        albedo_back += cl.color;
        break;
      case CLOSURE_NONE_ID:
        /* TODO(fclem): Assert. */
        break;
    }
  }

  out_radiance = vec4(radiance_front * albedo_front + radiance_back * albedo_back, 0.0);
}
