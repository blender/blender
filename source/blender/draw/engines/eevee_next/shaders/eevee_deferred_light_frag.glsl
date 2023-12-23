/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using Gbuffer data.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_renderpass_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_subsurface_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  if (gbuf.closure_count == 0) {
    return;
  }

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  /* Assume reflection closure normal is always somewhat representative of the geometric normal.
   * Ng is only used for shadow biases and subsurface check in this case. */
  vec3 Ng = gbuf.data.surface_N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureLight cl_diff;
  cl_diff.N = gbuf.data.diffuse.N;
  cl_diff.ltc_mat = LTC_LAMBERT_MAT;
  cl_diff.type = LIGHT_DIFFUSE;

  ClosureLight cl_refl;
  cl_refl.N = gbuf.data.reflection.N;
  cl_refl.ltc_mat = LTC_GGX_MAT(dot(gbuf.data.reflection.N, V), gbuf.data.reflection.roughness);
  cl_refl.type = LIGHT_SPECULAR;

  ClosureLight cl_sss;
  cl_sss.N = -gbuf.data.diffuse.N;
  cl_sss.ltc_mat = LTC_LAMBERT_MAT;
  cl_sss.type = LIGHT_DIFFUSE;

  ClosureLight cl_translucent;
  cl_translucent.N = -gbuf.data.translucent.N;
  cl_translucent.ltc_mat = LTC_LAMBERT_MAT;
  cl_translucent.type = LIGHT_DIFFUSE;

  ClosureLightStack stack;

  /* TODO(fclem): This is waiting for fully flexible evaluation pipeline. We need to refactor the
   * raytracing pipeline first. */
  stack.cl[0] = (gbuf.has_diffuse) ? cl_diff : cl_refl;

#if LIGHT_CLOSURE_EVAL_COUNT > 1
  stack.cl[1] = cl_refl;
#endif

#if LIGHT_CLOSURE_EVAL_COUNT > 2
  stack.cl[2] = (gbuf.has_translucent) ? cl_translucent : cl_sss;
#endif

  float thickness = (gbuf.has_translucent) ? gbuf.data.thickness : 0.0;
#ifdef MAT_SUBSURFACE
  if (gbuf.has_sss) {
    float shadow_thickness = thickness_from_shadow(P, Ng, vPz);
    thickness = (shadow_thickness != THICKNESS_NO_VALUE) ?
                    max(shadow_thickness, gbuf.data.thickness) :
                    gbuf.data.thickness;
  }
#endif

  light_eval(stack, P, Ng, V, vPz, thickness);

  vec3 radiance_shadowed = stack.cl[0].light_shadowed;
  vec3 radiance_unshadowed = stack.cl[0].light_unshadowed;
#if LIGHT_CLOSURE_EVAL_COUNT > 1
  radiance_shadowed += stack.cl[1].light_shadowed;
  radiance_unshadowed += stack.cl[1].light_unshadowed;
#endif
#if LIGHT_CLOSURE_EVAL_COUNT > 2
  radiance_shadowed += stack.cl[2].light_shadowed;
  radiance_unshadowed += stack.cl[2].light_unshadowed;
#endif

#ifdef MAT_SUBSURFACE
  if (gbuf.has_sss) {
    vec3 sss_profile = subsurface_transmission(gbuf.data.diffuse.sss_radius, thickness);
    stack.cl[2].light_shadowed *= sss_profile;
    stack.cl[2].light_unshadowed *= sss_profile;
    /* Add to diffuse light for processing inside the Screen Space SSS pass. */
    stack.cl[0].light_shadowed += stack.cl[2].light_shadowed;
    stack.cl[0].light_unshadowed += stack.cl[2].light_unshadowed;
  }
#endif

  /* TODO(fclem): Change shadow pass to be colored. */
  vec3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
  output_renderpass_value(uniform_buf.render_pass.shadow_id, average(shadows));

  if (gbuf.closure_count > 0) {
    /* TODO(fclem): This is waiting for fully flexible evaluation pipeline. We need to refactor the
     * raytracing pipeline first. */
    if (gbuf.has_diffuse) {
      imageStore(direct_radiance_1_img, texel, vec4(stack.cl[0].light_shadowed, 1.0));
    }
    else {
      imageStore(direct_radiance_2_img, texel, vec4(stack.cl[0].light_shadowed, 1.0));
    }
  }

#if LIGHT_CLOSURE_EVAL_COUNT > 1
  if (gbuf.closure_count > 1) {
    imageStore(direct_radiance_2_img, texel, vec4(stack.cl[1].light_shadowed, 1.0));
  }
#endif

#if LIGHT_CLOSURE_EVAL_COUNT > 2
  if (gbuf.closure_count > 2 || gbuf.has_translucent) {
    imageStore(direct_radiance_3_img, texel, vec4(stack.cl[2].light_shadowed, 1.0));
  }
#endif
}
