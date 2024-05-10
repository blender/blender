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
};

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
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      if (is_transmission) {
        cl_light.N = -cl.N;
        if (thickness != 0.0) {
          if (thickness > 0.0) {
            /* Strangely, a translucent sphere lit by a light outside the sphere transmits the
             * light uniformly over the sphere. To mimic this phenomenon, we use the light vector
             * as normal. */
            cl_light.N = vec3(0.0);
          }
          else {
            /* This approximation has little to no impact on the lighting in practice, only
             * focusing the light a tiny bit. Offset the shadow map position and using the flipped
             * normal is good enough approximation. */
          }
        }
      }
      break;
    case CLOSURE_BSSRDF_BURLEY_ID:
      if (is_transmission) {
        /* If the `thickness / sss_radius` ratio is near 0, this transmission term should converge
         * to a uniform term like the translucent BSDF. But we need to find what to do in other
         * cases. For now, approximate the transmission term as just back-facing. */
        cl_light.N = -cl.N;
      }
      else {
        /* Reflection term uses the lambertian diffuse. */
      }
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

        if (thickness != 0.0) {
          vec3 L = refraction_dominant_dir(cl.N, V, cl_refract.ior, cl_refract.roughness);

          ThicknessIsect isect = thickness_shape_intersect(thickness, cl.N, L);
          cl.N = -isect.hit_N;

          cl_refract.ior = 1.0 / cl_refract.ior;
          V = -L;
        }
        vec3 R = refract(-V, cl.N, 1.0 / cl_refract.ior);
        cl_light.ltc_mat = LTC_GGX_MAT(dot(-cl.N, R), cl_refract.roughness);
        cl_light.N = -cl.N;
        cl_light.type = LIGHT_TRANSMISSION;
      }
      break;
    }
    case CLOSURE_NONE_ID:
      /* Can happen in forward. */
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

  bool is_translucent_with_thickness = is_transmission && all(equal(stack.cl[0].N, vec3(0.0)));

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
                         lv.L,
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
