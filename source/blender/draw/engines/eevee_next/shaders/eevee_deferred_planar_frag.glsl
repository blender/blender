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

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  vec3 Ng = gbuf.diffuse.N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureLightStack stack;
  stack.cl[0].N = gbuf.diffuse.N;
  stack.cl[0].ltc_mat = LTC_LAMBERT_MAT;
  stack.cl[0].type = LIGHT_DIFFUSE;

  stack.cl[1].N = gbuf.reflection.N;
  stack.cl[1].ltc_mat = LTC_GGX_MAT(dot(gbuf.reflection.N, V), gbuf.reflection.roughness);
  stack.cl[1].type = LIGHT_SPECULAR;

  /* Direct light. */
  light_eval(stack, P, Ng, V, vPz, gbuf.thickness);
  /* Indirect light. */
  LightProbeSample samp = lightprobe_load(P, Ng, V);

  vec3 radiance = vec3(0.0);
  radiance += (stack.cl[0].light_shadowed + lightprobe_eval(samp, gbuf.diffuse, P, V, vec2(0.0))) *
              gbuf.diffuse.color;
  radiance += (stack.cl[1].light_shadowed +
               lightprobe_eval(samp, gbuf.reflection, P, V, vec2(0.0))) *
              gbuf.reflection.color;

  out_radiance = vec4(radiance, 0.0);
}
