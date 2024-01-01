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

ClosureLight closure_light_new(ClosureUndetermined cl, vec3 V)
{
  ClosureLight cl_light;
  cl_light.N = cl.N;
  cl_light.ltc_mat = LTC_LAMBERT_MAT;
  cl_light.type = LIGHT_DIFFUSE;
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      cl_light.N = -cl.N;
      break;
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      cl_light.ltc_mat = LTC_GGX_MAT(dot(cl.N, V), cl.data.x);
      cl_light.type = LIGHT_SPECULAR;
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      cl_light.type = LIGHT_SPECULAR;
      break;
    case CLOSURE_NONE_ID:
      /* TODO(fclem): Assert. */
      break;
  }
  return cl_light;
}

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  if (gbuf.closure_count == 0) {
    return;
  }

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  vec3 Ng = gbuf.data.surface_N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureLightStack stack;
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
    stack.cl[i] = closure_light_new(gbuf.closures[i], V);
  }

  /* TODO(fclem): Split thickness computation. */
  float thickness = (gbuf.has_translucent) ? gbuf.data.thickness : 0.0;
#ifdef MAT_SUBSURFACE
  if (gbuf.has_sss) {
    float shadow_thickness = thickness_from_shadow(P, Ng, vPz);
    thickness = (shadow_thickness != THICKNESS_NO_VALUE) ?
                    max(shadow_thickness, gbuf.data.thickness) :
                    gbuf.data.thickness;

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
  if (gbuf.has_sss) {
    for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
      /* Add to diffuse light for processing inside the Screen Space SSS pass.
       * The tranlucent light is not outputed as a separate quantity because
       * it is over the closure_count. */
      if (gbuf.closures[i].type == CLOSURE_BSSRDF_BURLEY_ID) {
        /* Apply absorption per closure. */
        vec3 sss_profile = subsurface_transmission(gbuf.closures[i].data.rgb, thickness);
        stack.cl[i].light_shadowed += stack.cl[gbuf.closure_count].light_shadowed * sss_profile;
        stack.cl[i].light_unshadowed += stack.cl[gbuf.closure_count].light_unshadowed *
                                        sss_profile;
      }
    }
  }
#endif

#if 1 /* TODO(fclem): Limit to when shadow pass is needed. */
  vec3 radiance_shadowed = vec3(0);
  vec3 radiance_unshadowed = vec3(0);
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
    radiance_shadowed += stack.cl[i].light_shadowed;
    radiance_unshadowed += stack.cl[i].light_unshadowed;
  }
  /* TODO(fclem): Change shadow pass to be colored. */
  vec3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
  output_renderpass_value(uniform_buf.render_pass.shadow_id, average(shadows));
#endif

  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < gbuf.closure_count; i++) {
    /* TODO(fclem): Layered texture. */
    if (i == 0) {
      imageStore(direct_radiance_1_img, texel, vec4(stack.cl[i].light_shadowed, 1.0));
    }
    else if (i == 1) {
      imageStore(direct_radiance_2_img, texel, vec4(stack.cl[i].light_shadowed, 1.0));
    }
    else if (i == 2) {
      imageStore(direct_radiance_3_img, texel, vec4(stack.cl[i].light_shadowed, 1.0));
    }
  }
}
