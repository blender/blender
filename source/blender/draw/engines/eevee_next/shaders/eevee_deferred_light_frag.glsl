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
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_subsurface_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)

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

  /* TODO(fclem): Split thickness computation. */
  float thickness = gbuf.thickness;
#ifdef MAT_SUBSURFACE
  /* NOTE: BSSRDF is supposed to always be the first closure. */
  bool has_sss = gbuffer_closure_get(gbuf, 0).type == CLOSURE_BSSRDF_BURLEY_ID;
  if (has_sss) {
    float shadow_thickness = thickness_from_shadow(P, Ng, vPz);
    thickness = (shadow_thickness != THICKNESS_NO_VALUE) ? max(shadow_thickness, gbuf.thickness) :
                                                           gbuf.thickness;

    /* Add one translucent closure for all SSS closure. Reuse the same lighting. */
    ClosureLight cl_light;
    cl_light.N = -Ng;
    cl_light.ltc_mat = LTC_LAMBERT_MAT;
    cl_light.type = LIGHT_DIFFUSE;
    stack.cl[gbuf.closure_count] = cl_light;
  }
#endif

  light_eval(stack, P, Ng, V, vPz, thickness);

#ifdef MAT_SUBSURFACE
  if (has_sss) {
    /* Add to diffuse light for processing inside the Screen Space SSS pass.
     * The translucent light is not outputted as a separate quantity because
     * it is over the closure_count. */
    vec3 sss_profile = subsurface_transmission(gbuffer_closure_get(gbuf, 0).data.rgb, thickness);
    stack.cl[0].light_shadowed += stack.cl[gbuf.closure_count].light_shadowed * sss_profile;
    stack.cl[0].light_unshadowed += stack.cl[gbuf.closure_count].light_unshadowed * sss_profile;
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
      lightprobe_eval(samp, cl, g_data.P, V, stack.cl[i].light_shadowed);
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
