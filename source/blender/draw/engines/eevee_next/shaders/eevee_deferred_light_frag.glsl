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
  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  /* Assume reflection closure normal is always somewhat representative of the geometric normal.
   * Ng is only used for shadow biases and subsurface check in this case. */
  vec3 Ng = gbuf.has_reflection ? gbuf.reflection.N : gbuf.diffuse.N;
  vec3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

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

#ifdef SSS_TRANSMITTANCE
  ClosureLight cl_sss;
  cl_sss.N = -gbuf.diffuse.N;
  cl_sss.ltc_mat = LTC_LAMBERT_MAT;
  cl_sss.type = LIGHT_DIFFUSE;
  stack.cl[2] = cl_sss;
#endif

#ifdef SSS_TRANSMITTANCE
  float shadow_thickness = thickness_from_shadow(P, Ng, vPz);
  float thickness = (shadow_thickness != THICKNESS_NO_VALUE) ?
                        max(shadow_thickness, gbuf.thickness) :
                        gbuf.thickness;
#else
  float thickness = 0.0;
#endif

  light_eval(stack, P, Ng, V, vPz, thickness);

#ifdef SSS_TRANSMITTANCE
  if (gbuf.diffuse.sss_id != 0u) {
    vec3 sss_profile = subsurface_transmission(gbuf.diffuse.sss_radius, thickness);
    stack.cl[2].light_shadowed *= sss_profile;
    stack.cl[2].light_unshadowed *= sss_profile;
  }
  else {
    stack.cl[2].light_shadowed = vec3(0.0);
    stack.cl[2].light_unshadowed = vec3(0.0);
  }
#endif

  vec3 radiance_diffuse = stack.cl[0].light_shadowed;
  vec3 radiance_specular = stack.cl[1].light_shadowed;
#ifdef SSS_TRANSMITTANCE
  radiance_diffuse += stack.cl[2].light_shadowed;
#endif

  vec3 radiance_shadowed = stack.cl[0].light_shadowed;
  vec3 radiance_unshadowed = stack.cl[0].light_unshadowed;
  radiance_shadowed += stack.cl[1].light_shadowed;
  radiance_unshadowed += stack.cl[1].light_unshadowed;
#ifdef SSS_TRANSMITTANCE
  radiance_shadowed += stack.cl[2].light_shadowed;
  radiance_unshadowed += stack.cl[2].light_unshadowed;
#endif

  /* TODO(fclem): Change shadow pass to be colored. */
  vec3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
  output_renderpass_value(uniform_buf.render_pass.shadow_id, average(shadows));

  imageStore(direct_diffuse_img, texel, vec4(radiance_diffuse, 1.0));
  imageStore(direct_reflect_img, texel, vec4(radiance_specular, 1.0));
  /* TODO(fclem): Support LTC for refraction. */
  // imageStore(direct_refract_img, texel, vec4(cl_refr.light_shadowed, 1.0));
}
