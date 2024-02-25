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
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

/* If using compute, the shader should define its own pixel. */
#if !defined(PIXEL) && defined(GPU_FRAGMENT_SHADER)
#  define PIXEL gl_FragCoord.xy
#endif

#if !defined(LIGHT_CLOSURE_EVAL_COUNT)
#  define LIGHT_CLOSURE_EVAL_COUNT 1
#  define SKIP_LIGHT_EVAL
#endif

uint shadow_pack(float visibility, uint bit_depth, uint shift)
{
  return uint(visibility * float((1u << bit_depth) - 1u)) << shift;
}

float shadow_unpack(uint shadow_bits, uint bit_depth, uint shift)
{
  return float((shadow_bits >> shift) & ~(~0u << bit_depth)) / float((1u << bit_depth) - 1u);
}

void light_shadow_single(uint l_idx,
                         const bool is_directional,
                         vec3 P,
                         vec3 Ng,
                         float thickness,
                         inout uint shadow_bits,
                         inout uint shift)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  bool use_subsurface = thickness > 0.0;
  LightVector lv = light_vector_get(light, is_directional, P);
  float attenuation = light_attenuation_surface(light, is_directional, Ng, use_subsurface, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return;
  }

#if defined(SPECIALIZED_SHADOW_PARAMS)
  int ray_count = shadow_ray_count;
  int ray_step_count = shadow_ray_step_count;
#else
  int ray_count = uniform_buf.shadow.ray_count;
  int ray_step_count = uniform_buf.shadow.step_count;
#endif

  ShadowEvalResult result = shadow_eval(
      light, is_directional, P, Ng, thickness, ray_count, ray_step_count);

  shadow_bits |= shadow_pack(result.light_visibilty, ray_count, shift);
  shift += ray_count;
}

void light_shadow_mask(vec3 P, vec3 Ng, float vPz, float thickness, out uint shadow_bits)
{
  int ray_count = uniform_buf.shadow.ray_count;
  int ray_step_count = uniform_buf.shadow.step_count;

  uint shift = 0u;
  shadow_bits = 0u;
  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_shadow_single(l_idx, true, P, Ng, thickness, shadow_bits, shift);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    light_shadow_single(l_idx, false, P, Ng, thickness, shadow_bits, shift);
  }
  LIGHT_FOREACH_END
}

struct ClosureLight {
  /* Shading normal. */
  vec3 N;
  /* LTC matrix. */
  vec4 ltc_mat;
  /* Enum (used as index) telling how to treat the lighting. */
  LightingType type;
  /* Output both shadowed and unshadowed for shadow denoising. */
  vec3 light_shadowed;
  vec3 light_unshadowed;
};

ClosureLight closure_light_new(ClosureUndetermined cl, vec3 V)
{
  ClosureLight cl_light;
  cl_light.N = cl.N;
  cl_light.ltc_mat = LTC_LAMBERT_MAT;
  cl_light.type = LIGHT_DIFFUSE;
  cl_light.light_shadowed = vec3(0.0);
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
      cl_light.N = -cl.N;
      cl_light.type = LIGHT_SPECULAR;
      break;
    case CLOSURE_NONE_ID:
      /* TODO(fclem): Assert. */
      break;
  }
  return cl_light;
}

struct ClosureLightStack {
  /* NOTE: This is wrapped into a struct to avoid array shenanigans on MSL. */
  ClosureLight cl[LIGHT_CLOSURE_EVAL_COUNT];
};

void light_eval_single_closure(LightData light,
                               LightVector lv,
                               inout ClosureLight cl,
                               vec3 P,
                               vec3 V,
                               float thickness,
                               float attenuation,
                               float visibility)
{
  if (light.power[cl.type] > 0.0) {
    float ltc_result = light_ltc(utility_tx, light, cl.N, V, lv, cl.ltc_mat);
    vec3 out_radiance = light.color * light.power[cl.type] * ltc_result;
    cl.light_shadowed += visibility * out_radiance;
    cl.light_unshadowed += attenuation * out_radiance;
  }
}

void light_eval_single(uint l_idx,
                       const bool is_directional,
                       inout ClosureLightStack stack,
                       vec3 P,
                       vec3 Ng,
                       vec3 V,
                       float thickness,
                       uint packed_shadows,
                       inout uint shift)
{
  LightData light = light_buf[l_idx];

#if defined(SPECIALIZED_SHADOW_PARAMS)
  int ray_count = shadow_ray_count;
  int ray_step_count = shadow_ray_step_count;
#else
  int ray_count = uniform_buf.shadow.ray_count;
  int ray_step_count = uniform_buf.shadow.step_count;
#endif

  bool use_subsurface = thickness > 0.0;
  LightVector lv = light_vector_get(light, is_directional, P);
  float attenuation = light_attenuation_surface(light, is_directional, Ng, use_subsurface, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return;
  }
  float shadow = 1.0;
  if (light.tilemap_index != LIGHT_NO_SHADOW) {
#ifdef SHADOW_DEFERRED
    shadow = shadow_unpack(packed_shadows, ray_count, shift);
    shift += ray_count;
#else
    ShadowEvalResult result = shadow_eval(
        light, is_directional, P, Ng, thickness, ray_count, ray_step_count);
    shadow = result.light_visibilty;
#endif
  }
  float visibility = attenuation * shadow;

  /* WATCH(@fclem): Might have to manually unroll for best performance. */
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    light_eval_single_closure(light, lv, stack.cl[i], P, V, thickness, attenuation, visibility);
  }
}

void light_eval(inout ClosureLightStack stack,
                vec3 P,
                vec3 Ng,
                vec3 V,
                float vPz,
                float thickness,
                uint packed_shadows)
{
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    stack.cl[i].light_shadowed = vec3(0.0);
    stack.cl[i].light_unshadowed = vec3(0.0);
  }

  uint shift = 0u;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_eval_single(l_idx, true, stack, P, Ng, V, thickness, packed_shadows, shift);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    light_eval_single(l_idx, false, stack, P, Ng, V, thickness, packed_shadows, shift);
  }
  LIGHT_FOREACH_END
}

/* Variations that have less arguments. */

#if !defined(SHADOW_DEFERRED)
void light_eval(inout ClosureLightStack stack, vec3 P, vec3 Ng, vec3 V, float vPz, float thickness)
{
  light_eval(stack, P, Ng, V, vPz, thickness, 0u);
}

#  if !defined(SHADOW_SUBSURFACE) && defined(LIGHT_ITER_FORCE_NO_CULLING)
void light_eval(inout ClosureLightStack stack, vec3 P, vec3 Ng, vec3 V)
{
  light_eval(stack, P, Ng, V, 0.0, 0.0, 0u);
}
#  endif

#endif
