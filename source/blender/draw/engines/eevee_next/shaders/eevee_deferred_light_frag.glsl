/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using Gbuffer data.
 *
 * Output light .
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_renderpass_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  vec3 P = get_world_space_from_depth(uvcoordsvar.xy, depth);
  vec3 V = cameraVec(P);

  ClosureLightStack stack;

  ClosureLight cl_diff;
  cl_diff.N = gbuf.diffuse.N;
  cl_diff.ltc_mat = LTC_LAMBERT_MAT;
  cl_diff.type = LIGHT_DIFFUSE;
  stack.cl[0] = cl_diff;

  ClosureLight cl_refl;
  cl_refl.N = gbuf.reflection.N;
  cl_refl.ltc_mat = LTC_GGX_MAT(dot(gbuf.reflection.N, V), gbuf.reflection.roughness);
  cl_refl.type = LIGHT_SPECULAR;
  stack.cl[1] = cl_refl;

  /* Assume reflection closure normal is always somewhat representative of the geometric normal.
   * Ng is only used for shadow biases and subsurface check in this case. */
  vec3 Ng = gbuf.has_reflection ? gbuf.reflection.N : gbuf.diffuse.N;
  float vPz = dot(cameraForward, P) - dot(cameraForward, cameraPos);

  light_eval(stack, P, Ng, V, vPz, gbuf.thickness);

  vec3 shadows = (stack.cl[0].light_shadowed + stack.cl[1].light_shadowed) /
                 (stack.cl[0].light_unshadowed + stack.cl[1].light_unshadowed);

  /* TODO(fclem): Change shadow pass to be colored. */
  output_renderpass_value(uniform_buf.render_pass.shadow_id, avg(shadows));

  imageStore(direct_diffuse_img, texel, vec4(stack.cl[0].light_shadowed, 1.0));
  imageStore(direct_reflect_img, texel, vec4(stack.cl[1].light_shadowed, 1.0));
  /* TODO(fclem): Support LTC for refraction. */
  // imageStore(direct_refract_img, texel, vec4(cl_refr.light_shadowed, 1.0));
}
