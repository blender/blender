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

#ifndef SKIP_LIGHT_EVAL
  light_eval(stack, g_data.P, g_data.Ng, V, vPz, thickness);
#endif

  LightProbeSample samp = lightprobe_load(g_data.P, g_data.Ng, V);

  /* Combine all radiance. */
  radiance = g_emission;
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get(i);
    lightprobe_eval(samp, cl, g_data.P, V, stack.cl[i].light_shadowed);
    if (cl.weight > 1e-5) {
      radiance += stack.cl[i].light_shadowed * cl.color * cl.weight;
    }
  }
  transmittance = g_transmittance;
}
