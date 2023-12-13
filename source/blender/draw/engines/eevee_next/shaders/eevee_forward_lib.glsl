/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#pragma BLENDER_REQUIRE(eevee_subsurface_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)

void forward_lighting_eval(float thickness, out vec3 radiance, out vec3 transmittance)
{
  float vPz = dot(drw_view_forward(), g_data.P) - dot(drw_view_forward(), drw_view_position());
  vec3 V = drw_world_incident_vector(g_data.P);

  ClosureLightStack stack;

  ClosureLight cl_diffuse;
  cl_diffuse.N = g_diffuse_data.N;
  cl_diffuse.ltc_mat = LTC_LAMBERT_MAT;
  cl_diffuse.type = LIGHT_DIFFUSE;

  ClosureLight cl_subsurface;
  cl_subsurface.N = -g_diffuse_data.N;
  cl_subsurface.ltc_mat = LTC_LAMBERT_MAT;
  cl_subsurface.type = LIGHT_DIFFUSE;

  ClosureLight cl_translucent;
  cl_translucent.N = -g_translucent_data.N;
  cl_translucent.ltc_mat = LTC_LAMBERT_MAT;
  cl_translucent.type = LIGHT_DIFFUSE;

  ClosureLight cl_reflection;
  cl_reflection.N = g_reflection_data.N;
  cl_reflection.ltc_mat = LTC_GGX_MAT(dot(g_reflection_data.N, V), g_reflection_data.roughness);
  cl_reflection.type = LIGHT_SPECULAR;

  int cl_layer = 0;

#ifdef MAT_DIFFUSE
  const int cl_diffuse_id = cl_layer++;
  stack.cl[cl_diffuse_id] = cl_diffuse;
#endif

#ifdef MAT_SUBSURFACE
  const int cl_subsurface_id = cl_layer++;
  stack.cl[cl_subsurface_id] = cl_subsurface;
#endif

#ifdef MAT_TRANSLUCENT
  const int cl_translucent_id = cl_layer++;
  stack.cl[cl_translucent_id] = cl_translucent;
#endif

#ifdef MAT_REFLECTION
  const int cl_reflection_id = cl_layer++;
  stack.cl[cl_reflection_id] = cl_reflection;
#endif

#ifndef SKIP_LIGHT_EVAL
  light_eval(stack, g_data.P, g_data.Ng, V, vPz, thickness);
#endif

#ifdef MAT_SUBSURFACE
  vec3 sss_profile = subsurface_transmission(g_diffuse_data.sss_radius, thickness);
  stack.cl[cl_subsurface_id].light_shadowed *= sss_profile;
  stack.cl[cl_subsurface_id].light_unshadowed *= sss_profile;
  /* Fuse back the SSS transmittance with the diffuse lighting. */
  stack.cl[cl_diffuse_id].light_shadowed += stack.cl[cl_subsurface_id].light_shadowed;
  stack.cl[cl_diffuse_id].light_unshadowed += stack.cl[cl_subsurface_id].light_unshadowed;
#endif

  vec3 diffuse_light = vec3(0.0);
  vec3 translucent_light = vec3(0.0);
  vec3 reflection_light = vec3(0.0);
  vec3 refraction_light = vec3(0.0);

  vec2 noise_probe = interlieved_gradient_noise(gl_FragCoord.xy, vec2(0, 1), vec2(0.0));
  LightProbeSample samp = lightprobe_load(g_data.P, g_data.Ng, V);

#ifdef MAT_DIFFUSE
  diffuse_light = stack.cl[cl_diffuse_id].light_shadowed;
  diffuse_light += lightprobe_eval(samp, g_diffuse_data, g_data.P, V, noise_probe);
#endif
#ifdef MAT_TRANSLUCENT
  translucent_light = stack.cl[cl_translucent_id].light_shadowed;
  translucent_light += lightprobe_eval(samp, g_translucent_data, g_data.P, V, noise_probe);
#endif
#ifdef MAT_REFLECTION
  reflection_light = stack.cl[cl_reflection_id].light_shadowed;
  reflection_light += lightprobe_eval(samp, g_reflection_data, g_data.P, V, noise_probe);
#endif
#ifdef MAT_REFRACTION
  /* TODO(fclem): Refraction from lightprobe. */
  // refraction_light += lightprobe_eval(samp, g_refraction_data, g_data.P, V, noise_probe);
#endif

  /* Apply weight. */
  g_diffuse_data.color *= g_diffuse_data.weight;
  g_translucent_data.color *= g_translucent_data.weight;
  g_reflection_data.color *= g_reflection_data.weight;
  g_refraction_data.color *= g_refraction_data.weight;
  /* Mask invalid lighting from undefined closure. */
  diffuse_light = (g_diffuse_data.weight > 1e-5) ? diffuse_light : vec3(0.0);
  translucent_light = (g_translucent_data.weight > 1e-5) ? translucent_light : vec3(0.0);
  reflection_light = (g_reflection_data.weight > 1e-5) ? reflection_light : vec3(0.0);
  refraction_light = (g_refraction_data.weight > 1e-5) ? refraction_light : vec3(0.0);

  /* Combine all radiance. */
  radiance = g_emission;
  radiance += g_diffuse_data.color * diffuse_light;
  radiance += g_reflection_data.color * reflection_light;
  radiance += g_refraction_data.color * refraction_light;
  radiance += g_translucent_data.color * translucent_light;

  transmittance = g_transmittance;
}
