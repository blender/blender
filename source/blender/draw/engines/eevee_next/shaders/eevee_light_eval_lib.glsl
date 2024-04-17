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
                         const bool is_transmission,
                         vec3 P,
                         vec3 Ng,
                         inout uint shadow_bits,
                         inout uint shift)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  LightVector lv = light_vector_get(light, is_directional, P);
  float attenuation = light_attenuation_surface(
      light, is_directional, is_transmission, false, Ng, lv);
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
      light, is_directional, is_transmission, false, P, Ng, Ng, ray_count, ray_step_count);

  shadow_bits |= shadow_pack(result.light_visibilty, ray_count, shift);
  shift += ray_count;
}

void light_shadow_mask(vec3 P, vec3 Ng, float vPz, out uint shadow_bits)
{
  int ray_count = uniform_buf.shadow.ray_count;
  int ray_step_count = uniform_buf.shadow.step_count;

  uint shift = 0u;
  shadow_bits = 0u;
  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_shadow_single(l_idx, true, false, P, Ng, shadow_bits, shift);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    light_shadow_single(l_idx, false, false, P, Ng, shadow_bits, shift);
  }
  LIGHT_FOREACH_END
}

struct ClosureLight {
  /* LTC matrix. */
  vec4 ltc_mat;
  /* Shading normal. */
  vec3 N;
  /* Enum used as index to fetch which light intensity to use [0..3]. */
  LightingType type;
  /* Output both shadowed and unshadowed for shadow denoising. */
  vec3 light_shadowed;
  vec3 light_unshadowed;

  /** Only for transmission BSDFs. */
  vec3 shading_offset;
  float shadow_offset;
};

struct ClosureLightStack {
  /* NOTE: This is wrapped into a struct to avoid array shenanigans on MSL. */
  ClosureLight cl[LIGHT_CLOSURE_EVAL_COUNT];
};

ClosureLight closure_light_new_ex(ClosureUndetermined cl,
                                  vec3 V,
                                  float thickness,
                                  const bool is_transmission)
{
  ClosureLight cl_light;
  cl_light.N = cl.N;
  cl_light.ltc_mat = LTC_LAMBERT_MAT;
  cl_light.type = LIGHT_DIFFUSE;
  cl_light.light_shadowed = vec3(0.0);
  cl_light.light_unshadowed = vec3(0.0);
  cl_light.shading_offset = vec3(0.0);
  cl_light.shadow_offset = is_transmission ? thickness : 0.0;
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      if (is_transmission) {
        cl_light.N = -cl.N;
        cl_light.type = LIGHT_DIFFUSE;

        if (thickness > 0.0) {
          /* Strangely, a translucent sphere lit by a light outside the sphere transmits the light
           * uniformly over the sphere. To mimic this phenomenon, we shift the shading position to
           * a unique position on the sphere and use the light vector as normal. */
          cl_light.shading_offset = -cl.N * thickness * 0.5;
          cl_light.N = vec3(0.0);
        }
      }
      break;
    case CLOSURE_BSSRDF_BURLEY_ID:
      if (is_transmission) {
        /* If the `thickness / sss_radius` ratio is near 0, this transmission term should converge
         * to a uniform term like the translucent BSDF. But we need to find what to do in other
         * cases. For now, approximate the transmission term as just back-facing. */
        cl_light.N = -cl.N;
        cl_light.type = LIGHT_DIFFUSE;
        /* Lit and shadow as outside of the object. */
        cl_light.shading_offset = -cl.N * thickness;
      }
      /* Reflection term uses the lambertian diffuse. */
      break;
    case CLOSURE_BSDF_DIFFUSE_ID:
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      cl_light.ltc_mat = LTC_GGX_MAT(dot(cl.N, V), cl.data.x);
      cl_light.type = LIGHT_SPECULAR;
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
      if (is_transmission) {
        ClosureRefraction cl_refract = to_closure_refraction(cl);
        cl_refract.roughness = refraction_roughness_remapping(cl_refract.roughness,
                                                              cl_refract.ior);

        if (thickness > 0.0) {
          vec3 L = refraction_dominant_dir(cl.N, V, cl_refract.ior, cl_refract.roughness);

          ThicknessIsect isect = thickness_sphere_intersect(thickness, cl.N, L);
          cl.N = -isect.hit_N;
          cl_light.shading_offset = isect.hit_P;

          cl_refract.ior = 1.0 / cl_refract.ior;
          V = -L;
        }
        vec3 R = refract(-V, cl.N, 1.0 / cl_refract.ior);
        cl_light.ltc_mat = LTC_GGX_MAT(dot(-cl.N, R), cl_refract.roughness);
        cl_light.N = -cl.N;
        cl_light.type = LIGHT_SPECULAR;
      }
      break;
    }
    case CLOSURE_NONE_ID:
      /* TODO(fclem): Assert. */
      break;
  }
  return cl_light;
}

ClosureLight closure_light_new(ClosureUndetermined cl, vec3 V, float thickness)
{
  return closure_light_new_ex(cl, V, thickness, true);
}

ClosureLight closure_light_new(ClosureUndetermined cl, vec3 V)
{
  return closure_light_new_ex(cl, V, 0.0, false);
}

void light_eval_single_closure(LightData light,
                               LightVector lv,
                               inout ClosureLight cl,
                               vec3 V,
                               float attenuation,
                               float shadow,
                               const bool is_transmission)
{
  if (light.power[cl.type] > 0.0) {
    float ltc_result = light_ltc(utility_tx, light, cl.N, V, lv, cl.ltc_mat);
    vec3 out_radiance = light.color * light.power[cl.type] * ltc_result;
    float visibility = shadow * attenuation;
    cl.light_shadowed += visibility * out_radiance;
    cl.light_unshadowed += attenuation * out_radiance;
  }
}

void light_eval_single(uint l_idx,
                       const bool is_directional,
                       const bool is_transmission,
                       inout ClosureLightStack stack,
                       vec3 P,
                       vec3 Ng,
                       vec3 V,
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

  vec3 shading_P = (is_transmission) ? P + stack.cl[0].shading_offset : P;
  LightVector lv = light_vector_get(light, is_directional, shading_P);

  bool is_translucent_with_thickness = is_transmission && all(equal(stack.cl[0].N, vec3(0.0)));

  float attenuation = light_attenuation_surface(
      light, is_directional, is_transmission, is_translucent_with_thickness, Ng, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return;
  }

  float shadow = 1.0;
  if (light.tilemap_index != LIGHT_NO_SHADOW) {
#ifdef SHADOW_DEFERRED
    shadow = shadow_unpack(packed_shadows, ray_count, shift);
    shift += ray_count;
#else

    vec3 shadow_P = (is_transmission) ? P + lv.L * stack.cl[0].shadow_offset : P;
    ShadowEvalResult result = shadow_eval(light,
                                          is_directional,
                                          is_transmission,
                                          is_translucent_with_thickness,
                                          shadow_P,
                                          Ng,
                                          lv.L,
                                          ray_count,
                                          ray_step_count);
    shadow = result.light_visibilty;
#endif
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

void light_eval_transmission(inout ClosureLightStack stack, vec3 P, vec3 Ng, vec3 V, float vPz)
{
#ifdef SKIP_LIGHT_EVAL
  return;
#endif
  /* Packed / Decoupled shadow evaluation. Not yet implemented. */
  uint packed_shadows = 0u;
  uint shift = 0u;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_eval_single(l_idx, true, true, stack, P, Ng, V, packed_shadows, shift);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    light_eval_single(l_idx, false, true, stack, P, Ng, V, packed_shadows, shift);
  }
  LIGHT_FOREACH_END
}

void light_eval_reflection(inout ClosureLightStack stack, vec3 P, vec3 Ng, vec3 V, float vPz)
{
#ifdef SKIP_LIGHT_EVAL
  return;
#endif
  /* Packed / Decoupled shadow evaluation. Not yet implemented. */
  uint packed_shadows = 0u;
  uint shift = 0u;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_eval_single(l_idx, true, false, stack, P, Ng, V, packed_shadows, shift);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    light_eval_single(l_idx, false, false, stack, P, Ng, V, packed_shadows, shift);
  }
  LIGHT_FOREACH_END
}
