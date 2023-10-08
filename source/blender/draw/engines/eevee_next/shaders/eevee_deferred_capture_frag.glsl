/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using captured Gbuffer data.
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  ClosureLightStack stack;
  stack.cl[0].N = gbuf.has_diffuse ? gbuf.diffuse.N : gbuf.reflection.N;
  stack.cl[0].ltc_mat = LTC_LAMBERT_MAT;
  stack.cl[0].type = LIGHT_DIFFUSE;

  vec3 P = get_world_space_from_depth(uvcoordsvar.xy, depth);
  vec3 Ng = gbuf.diffuse.N;
  vec3 V = cameraVec(P);
  float vPz = dot(cameraForward, P) - dot(cameraForward, cameraPos);
  light_eval(stack, P, Ng, V, vPz, gbuf.thickness);

  vec3 albedo = gbuf.diffuse.color + gbuf.reflection.color + gbuf.refraction.color;

  out_radiance = vec4(stack.cl[0].light_shadowed * albedo, 0.0);
}
