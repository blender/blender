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

void write_radiance_direct(int layer_index, ivec2 texel, vec3 radiance)
{
  /* TODO(fclem): Layered texture. */
  if (layer_index == 0) {
    imageStore(direct_radiance_1_img, texel, vec4(radiance, 1.0));
  }
  else if (layer_index == 1) {
    imageStore(direct_radiance_2_img, texel, vec4(radiance, 1.0));
  }
  else if (layer_index == 2) {
    imageStore(direct_radiance_3_img, texel, vec4(radiance, 1.0));
  }
}

void write_radiance_indirect(int layer_index, ivec2 texel, vec3 radiance)
{
  /* TODO(fclem): Layered texture. */
  if (layer_index == 0) {
    imageStore(indirect_radiance_1_img, texel, vec4(radiance, 1.0));
  }
  else if (layer_index == 1) {
    imageStore(indirect_radiance_2_img, texel, vec4(radiance, 1.0));
  }
  else if (layer_index == 2) {
    imageStore(indirect_radiance_3_img, texel, vec4(radiance, 1.0));
  }
}

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  /* Bias the shading point position because of depth buffer precision.
   * Constant is taken from https://www.terathon.com/gdc07_lengyel.pdf. */
  const float bias = 2.4e-7;
  depth -= bias;

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  vec3 Ng = gbuf.surface_N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureLightStack stack;
  /* Unroll light stack array assignments to avoid non-constant indexing. */
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
    closure_light_set(stack, i, closure_light_new(gbuffer_closure_get(gbuf, i), V));
  }

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  light_eval_reflection(stack, P, Ng, V, vPz);

#if 1 /* TODO Limit to transmission. Can bypass the check if stencil is tagged properly and use \
         specialization constant. */
  ClosureUndetermined cl_transmit = gbuffer_closure_get(gbuf, 0);
  if ((cl_transmit.type == CLOSURE_BSDF_TRANSLUCENT_ID) ||
      (cl_transmit.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) ||
      (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID))
  {

#  if 1 /* TODO Limit to SSS. */
    vec3 sss_reflect_shadowed, sss_reflect_unshadowed;
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      sss_reflect_shadowed = stack.cl[0].light_shadowed;
      sss_reflect_unshadowed = stack.cl[0].light_unshadowed;
    }
#  endif

    stack.cl[0] = closure_light_new(cl_transmit, V, gbuf.thickness);

    /* NOTE: Only evaluates `stack.cl[0]`. */
    light_eval_transmission(stack, P, Ng, V, vPz, gbuf.thickness);

#  if 1 /* TODO Limit to SSS. */
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      /* Apply transmission profile onto transmitted light and sum with reflected light. */
      vec3 sss_profile = subsurface_transmission(to_closure_subsurface(cl_transmit).sss_radius,
                                                 abs(gbuf.thickness));
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
      radiance_shadowed += closure_light_get(stack, i).light_shadowed;
      radiance_unshadowed += closure_light_get(stack, i).light_unshadowed;
    }
    /* TODO(fclem): Change shadow pass to be colored. */
    vec3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
    output_renderpass_value(uniform_buf.render_pass.shadow_id, average(shadows));
  }

  if (use_lightprobe_eval) {
    LightProbeSample samp = lightprobe_load(P, Ng, V);

    float clamp_indirect = uniform_buf.clamp.surface_indirect;
    samp.volume_irradiance = spherical_harmonics_clamp(samp.volume_irradiance, clamp_indirect);

    for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      ClosureUndetermined cl = gbuffer_closure_get(gbuf, i);
      vec3 indirect_light = lightprobe_eval(samp, cl, P, V, gbuf.thickness);

      int layer_index = gbuffer_closure_get_bin_index(gbuf, i);
      vec3 direct_light = closure_light_get(stack, i).light_shadowed;
      if (use_split_indirect) {
        write_radiance_indirect(layer_index, texel, indirect_light);
        write_radiance_direct(layer_index, texel, direct_light);
      }
      else {
        write_radiance_direct(layer_index, texel, direct_light + indirect_light);
      }
    }
  }
  else {
    for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      int layer_index = gbuffer_closure_get_bin_index(gbuf, i);
      vec3 direct_light = closure_light_get(stack, i).light_shadowed;
      write_radiance_direct(layer_index, texel, direct_light);
    }
  }
}
