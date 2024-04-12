/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using Gbuffer data.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_renderpass_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_subsurface_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_amend_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  vec3 Ng = gbuf.surface_N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureLightStack stack;
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
    stack.cl[i] = closure_light_new(gbuffer_closure_get(gbuf, i), V);
  }

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluaiton and skip evaluating the transmission closure twice. */
  light_eval_reflection(stack, P, Ng, V, vPz);

#if 1 /* TODO Limit to transmission. Can bypass the check if stencil is tagged properly and use \
         specialization constant. */
  ClosureUndetermined cl_transmit = gbuffer_closure_get(gbuf, 0);
  if ((cl_transmit.type == CLOSURE_BSDF_TRANSLUCENT_ID) ||
      (cl_transmit.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) ||
      (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID))
  {
    float shadow_thickness = thickness_from_shadow(P, Ng, vPz);
    gbuf.thickness = (shadow_thickness != THICKNESS_NO_VALUE) ?
                         min(shadow_thickness, gbuf.thickness) :
                         gbuf.thickness;

#  if 1 /* TODO Limit to SSS. */
    vec3 sss_reflect_shadowed, sss_reflect_unshadowed;
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      sss_reflect_shadowed = stack.cl[0].light_shadowed;
      sss_reflect_unshadowed = stack.cl[0].light_unshadowed;
    }
#  endif

    vec3 P_transmit = vec3(0.0);
    stack.cl[0] = closure_light_new(cl_transmit, V, P, gbuf.thickness, P_transmit);

    /* Note: Only evaluates `stack.cl[0]`. */
    light_eval_transmission(stack, P_transmit, Ng, V, vPz);

#  if 1 /* TODO Limit to SSS. */
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      /* Apply transmission profile onto transmitted light and sum with reflected light. */
      vec3 sss_profile = subsurface_transmission(to_closure_subsurface(cl_transmit).sss_radius,
                                                 gbuf.thickness);
      stack.cl[0].light_shadowed *= sss_profile;
      stack.cl[0].light_unshadowed *= sss_profile;
      stack.cl[0].light_shadowed += sss_reflect_shadowed;
      stack.cl[0].light_unshadowed += sss_reflect_unshadowed;
    }
#  endif
  }
#endif

  if (render_pass_shadow_enabled) {
    vec3 radiance_shadowed = vec3(0);
    vec3 radiance_unshadowed = vec3(0);
    for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      radiance_shadowed += stack.cl[i].light_shadowed;
      radiance_unshadowed += stack.cl[i].light_unshadowed;
    }
    /* TODO(fclem): Change shadow pass to be colored. */
    vec3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
    output_renderpass_value(uniform_buf.render_pass.shadow_id, average(shadows));
  }

  if (use_lightprobe_eval) {
    LightProbeSample samp = lightprobe_load(P, Ng, V);

    for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      ClosureUndetermined cl = gbuffer_closure_get(gbuf, i);
      lightprobe_eval(samp, cl, g_data.P, V, gbuf.thickness, stack.cl[i].light_shadowed);
    }
  }

  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
    int layer_index = gbuffer_closure_get_bin_index(gbuf, i);
    /* TODO(fclem): Layered texture. */
    if (layer_index == 0) {
      imageStore(direct_radiance_1_img, texel, vec4(stack.cl[i].light_shadowed, 1.0));
    }
    else if (layer_index == 1) {
      imageStore(direct_radiance_2_img, texel, vec4(stack.cl[i].light_shadowed, 1.0));
    }
    else if (layer_index == 2) {
      imageStore(direct_radiance_3_img, texel, vec4(stack.cl[i].light_shadowed, 1.0));
    }
  }
}
