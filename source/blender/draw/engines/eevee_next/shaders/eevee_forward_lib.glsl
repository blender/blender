/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_subsurface_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)

#if CLOSURE_BIN_COUNT != LIGHT_CLOSURE_EVAL_COUNT
#  error Closure data count and eval count must match
#endif

void forward_lighting_eval(float thickness, out vec3 radiance, out vec3 transmittance)
{
  float vPz = dot(drw_view_forward(), g_data.P) - dot(drw_view_forward(), drw_view_position());
  vec3 V = drw_world_incident_vector(g_data.P);

  ClosureLightStack stack;
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    stack.cl[i] = closure_light_new(g_closure_get(i), V);
  }

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluaiton and skip evaluating the transmission closure twice. */
  light_eval_reflection(stack, g_data.P, g_data.Ng, V, vPz);

#if defined(MAT_SUBSURFACE) || defined(MAT_REFRACTION) || defined(MAT_TRANSLUCENT)

  ClosureUndetermined cl_transmit = g_closure_get(0);
  if (cl_transmit.type != CLOSURE_NONE) {
#  if defined(MAT_SUBSURFACE)
    vec3 sss_reflect_shadowed, sss_reflect_unshadowed;
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      sss_reflect_shadowed = stack.cl[0].light_shadowed;
      sss_reflect_unshadowed = stack.cl[0].light_unshadowed;
    }
#  endif

    stack.cl[0] = closure_light_new(cl_transmit, V, thickness);

    /* Note: Only evaluates `stack.cl[0]`. */
    light_eval_transmission(stack, g_data.P, g_data.Ng, V, vPz);

#  if defined(MAT_SUBSURFACE)
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

  LightProbeSample samp = lightprobe_load(g_data.P, g_data.Ng, V);

  /* Combine all radiance. */
  radiance = g_emission;
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get(i);
    lightprobe_eval(samp, cl, g_data.P, V, thickness, stack.cl[i].light_shadowed);
    if (cl.weight > 1e-5) {
      if ((cl.type == CLOSURE_BSDF_TRANSLUCENT_ID ||
           cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) &&
          (thickness > 0.0))
      {
        /* We model two transmission event, so the surface color need to be applied twice. */
        stack.cl[i].light_shadowed *= cl.color;
      }
      radiance += stack.cl[i].light_shadowed * cl.color * cl.weight;
    }
  }
  transmittance = g_transmittance;
}
