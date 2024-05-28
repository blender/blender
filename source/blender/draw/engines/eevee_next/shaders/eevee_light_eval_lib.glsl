/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * The resources expected to be defined are:
 * - light_buf
 * - light_zbin_buf
 * - light_cull_buf
 * - light_tile_buf
 * - shadow_atlas_tx
 * - shadow_tilemaps_tx
 * - utility_tx
 */

#pragma BLENDER_REQUIRE(eevee_shadow_tracing_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_closure_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

/* If using compute, the shader should define its own pixel. */
#if !defined(PIXEL) && defined(GPU_FRAGMENT_SHADER)
#  define PIXEL gl_FragCoord.xy
#endif

#if !defined(LIGHT_CLOSURE_EVAL_COUNT)
#  define LIGHT_CLOSURE_EVAL_COUNT 1
#  define SKIP_LIGHT_EVAL
#endif

struct ClosureLightStack {
  /* NOTE: This is wrapped into a struct to avoid array shenanigans on MSL. */
  ClosureLight cl[LIGHT_CLOSURE_EVAL_COUNT];
};

ClosureLight closure_light_get(ClosureLightStack stack, int index)
{
  switch (index) {
    case 0:
      return stack.cl[0];
#if LIGHT_CLOSURE_EVAL_COUNT > 1
    case 1:
      return stack.cl[1];
#endif
#if LIGHT_CLOSURE_EVAL_COUNT > 2
    case 2:
      return stack.cl[2];
#endif
#if LIGHT_CLOSURE_EVAL_COUNT > 3
#  error
#endif
  }
  ClosureLight closure_null;
  return closure_null;
}

void closure_light_set(inout ClosureLightStack stack, int index, ClosureLight cl_light)
{
  switch (index) {
    case 0:
      stack.cl[0] = cl_light;
#if LIGHT_CLOSURE_EVAL_COUNT > 1
    case 1:
      stack.cl[1] = cl_light;
#endif
#if LIGHT_CLOSURE_EVAL_COUNT > 2
    case 2:
      stack.cl[2] = cl_light;
#endif
#if LIGHT_CLOSURE_EVAL_COUNT > 3
#  error
#endif
  }
}

float light_power_get(LightData light, LightingType type)
{
  /* Mask anything above 3. See LIGHT_TRANSLUCENT_WITH_THICKNESS. */
  return light.power[type & 3u];
}

void light_eval_single_closure(LightData light,
                               LightVector lv,
                               inout ClosureLight cl,
                               vec3 V,
                               float attenuation,
                               float shadow,
                               const bool is_transmission)
{
  attenuation *= light_power_get(light, cl.type);
  if (attenuation < 1e-5) {
    return;
  }
  float ltc_result = light_ltc(utility_tx, light, cl.N, V, lv, cl.ltc_mat);
  vec3 out_radiance = light.color * ltc_result;
  float visibility = shadow * attenuation;
  cl.light_shadowed += visibility * out_radiance;
  cl.light_unshadowed += attenuation * out_radiance;
}

void light_eval_single(uint l_idx,
                       const bool is_directional,
                       const bool is_transmission,
                       inout ClosureLightStack stack,
                       vec3 P,
                       vec3 Ng,
                       vec3 V,
                       float thickness)
{
  LightData light = light_buf[l_idx];

#if defined(SPECIALIZED_SHADOW_PARAMS)
  int ray_count = shadow_ray_count;
  int ray_step_count = shadow_ray_step_count;
#else
  int ray_count = uniform_buf.shadow.ray_count;
  int ray_step_count = uniform_buf.shadow.step_count;
#endif

  LightVector lv = light_vector_get(light, is_directional, P);

  /* TODO(fclem): Get rid of this special case. */
  bool is_translucent_with_thickness = is_transmission &&
                                       (stack.cl[0].type == LIGHT_TRANSLUCENT_WITH_THICKNESS);

  float attenuation = light_attenuation_surface(
      light, is_directional, is_transmission, is_translucent_with_thickness, Ng, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return;
  }

  float shadow = 1.0;
  if (light.tilemap_index != LIGHT_NO_SHADOW) {
    shadow = shadow_eval(light,
                         is_directional,
                         is_transmission,
                         is_translucent_with_thickness,
                         thickness,
                         P,
                         Ng,
                         ray_count,
                         ray_step_count);
  }

  if (is_translucent_with_thickness) {
    /* This makes the LTC compute the solid angle of the light (still with the cosine term applied
     * but that still works great enough in practice). */
    stack.cl[0].N = lv.L;
    /* Adjust power because of the second lambertian distribution. */
    attenuation *= M_1_PI;
  }

  light_eval_single_closure(light, lv, stack.cl[0], V, attenuation, shadow, is_transmission);
  if (!is_transmission) {
#if LIGHT_CLOSURE_EVAL_COUNT > 1
    light_eval_single_closure(light, lv, stack.cl[1], V, attenuation, shadow, is_transmission);
#endif
#if LIGHT_CLOSURE_EVAL_COUNT > 2
    light_eval_single_closure(light, lv, stack.cl[2], V, attenuation, shadow, is_transmission);
#endif
#if LIGHT_CLOSURE_EVAL_COUNT > 3
#  error
#endif
  }
}

void light_eval_transmission(
    inout ClosureLightStack stack, vec3 P, vec3 Ng, vec3 V, float vPz, float thickness)
{
#ifdef SKIP_LIGHT_EVAL
  return;
#endif

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_eval_single(l_idx, true, true, stack, P, Ng, V, thickness);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    light_eval_single(l_idx, false, true, stack, P, Ng, V, thickness);
  }
  LIGHT_FOREACH_END
}

void light_eval_reflection(inout ClosureLightStack stack, vec3 P, vec3 Ng, vec3 V, float vPz)
{
#ifdef SKIP_LIGHT_EVAL
  return;
#endif

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_eval_single(l_idx, true, false, stack, P, Ng, V, 0.0);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    light_eval_single(l_idx, false, false, stack, P, Ng, V, 0.0);
  }
  LIGHT_FOREACH_END
}
