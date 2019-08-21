/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(__SPLIT_KERNEL__) || defined(__KERNEL_CUDA__)
#  define __ATOMIC_PASS_WRITE__
#endif

#include "kernel/kernel_id_passes.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void kernel_write_pass_float(ccl_global float *buffer, float value)
{
  ccl_global float *buf = buffer;
#ifdef __ATOMIC_PASS_WRITE__
  atomic_add_and_fetch_float(buf, value);
#else
  *buf += value;
#endif
}

ccl_device_inline void kernel_write_pass_float3(ccl_global float *buffer, float3 value)
{
#ifdef __ATOMIC_PASS_WRITE__
  ccl_global float *buf_x = buffer + 0;
  ccl_global float *buf_y = buffer + 1;
  ccl_global float *buf_z = buffer + 2;

  atomic_add_and_fetch_float(buf_x, value.x);
  atomic_add_and_fetch_float(buf_y, value.y);
  atomic_add_and_fetch_float(buf_z, value.z);
#else
  ccl_global float3 *buf = (ccl_global float3 *)buffer;
  *buf += value;
#endif
}

ccl_device_inline void kernel_write_pass_float4(ccl_global float *buffer, float4 value)
{
#ifdef __ATOMIC_PASS_WRITE__
  ccl_global float *buf_x = buffer + 0;
  ccl_global float *buf_y = buffer + 1;
  ccl_global float *buf_z = buffer + 2;
  ccl_global float *buf_w = buffer + 3;

  atomic_add_and_fetch_float(buf_x, value.x);
  atomic_add_and_fetch_float(buf_y, value.y);
  atomic_add_and_fetch_float(buf_z, value.z);
  atomic_add_and_fetch_float(buf_w, value.w);
#else
  ccl_global float4 *buf = (ccl_global float4 *)buffer;
  *buf += value;
#endif
}

#ifdef __DENOISING_FEATURES__
ccl_device_inline void kernel_write_pass_float_variance(ccl_global float *buffer, float value)
{
  kernel_write_pass_float(buffer, value);

  /* The online one-pass variance update that's used for the mega-kernel can't easily be
   * implemented with atomics,
   * so for the split kernel the E[x^2] - 1/N * (E[x])^2 fallback is used. */
  kernel_write_pass_float(buffer + 1, value * value);
}

#  ifdef __ATOMIC_PASS_WRITE__
#    define kernel_write_pass_float3_unaligned kernel_write_pass_float3
#  else
ccl_device_inline void kernel_write_pass_float3_unaligned(ccl_global float *buffer, float3 value)
{
  buffer[0] += value.x;
  buffer[1] += value.y;
  buffer[2] += value.z;
}
#  endif

ccl_device_inline void kernel_write_pass_float3_variance(ccl_global float *buffer, float3 value)
{
  kernel_write_pass_float3_unaligned(buffer, value);
  kernel_write_pass_float3_unaligned(buffer + 3, value * value);
}

ccl_device_inline void kernel_write_denoising_shadow(KernelGlobals *kg,
                                                     ccl_global float *buffer,
                                                     int sample,
                                                     float path_total,
                                                     float path_total_shaded)
{
  if (kernel_data.film.pass_denoising_data == 0)
    return;

  buffer += (sample & 1) ? DENOISING_PASS_SHADOW_B : DENOISING_PASS_SHADOW_A;

  path_total = ensure_finite(path_total);
  path_total_shaded = ensure_finite(path_total_shaded);

  kernel_write_pass_float(buffer, path_total);
  kernel_write_pass_float(buffer + 1, path_total_shaded);

  float value = path_total_shaded / max(path_total, 1e-7f);
  kernel_write_pass_float(buffer + 2, value * value);
}

ccl_device_inline void kernel_update_denoising_features(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        ccl_addr_space PathState *state,
                                                        PathRadiance *L)
{
  if (state->denoising_feature_weight == 0.0f) {
    return;
  }

  L->denoising_depth += ensure_finite(state->denoising_feature_weight * sd->ray_length);

  /* Skip implicitly transparent surfaces. */
  if (sd->flag & SD_HAS_ONLY_VOLUME) {
    return;
  }

  float3 normal = make_float3(0.0f, 0.0f, 0.0f);
  float3 albedo = make_float3(0.0f, 0.0f, 0.0f);
  float sum_weight = 0.0f, sum_nonspecular_weight = 0.0f;

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (!CLOSURE_IS_BSDF_OR_BSSRDF(sc->type))
      continue;

    /* All closures contribute to the normal feature, but only diffuse-like ones to the albedo. */
    normal += sc->N * sc->sample_weight;
    sum_weight += sc->sample_weight;
    if (bsdf_get_specular_roughness_squared(sc) > sqr(0.075f)) {
      albedo += sc->weight;
      sum_nonspecular_weight += sc->sample_weight;
    }
  }

  /* Wait for next bounce if 75% or more sample weight belongs to specular-like closures. */
  if ((sum_weight == 0.0f) || (sum_nonspecular_weight * 4.0f > sum_weight)) {
    if (sum_weight != 0.0f) {
      normal /= sum_weight;
    }
    L->denoising_normal += ensure_finite3(state->denoising_feature_weight * normal);
    L->denoising_albedo += ensure_finite3(state->denoising_feature_weight * albedo);

    state->denoising_feature_weight = 0.0f;
  }
}
#endif /* __DENOISING_FEATURES__ */

#ifdef __KERNEL_DEBUG__
ccl_device_inline void kernel_write_debug_passes(KernelGlobals *kg,
                                                 ccl_global float *buffer,
                                                 PathRadiance *L)
{
  int flag = kernel_data.film.pass_flag;
  if (flag & PASSMASK(BVH_TRAVERSED_NODES)) {
    kernel_write_pass_float(buffer + kernel_data.film.pass_bvh_traversed_nodes,
                            L->debug_data.num_bvh_traversed_nodes);
  }
  if (flag & PASSMASK(BVH_TRAVERSED_INSTANCES)) {
    kernel_write_pass_float(buffer + kernel_data.film.pass_bvh_traversed_instances,
                            L->debug_data.num_bvh_traversed_instances);
  }
  if (flag & PASSMASK(BVH_INTERSECTIONS)) {
    kernel_write_pass_float(buffer + kernel_data.film.pass_bvh_intersections,
                            L->debug_data.num_bvh_intersections);
  }
  if (flag & PASSMASK(RAY_BOUNCES)) {
    kernel_write_pass_float(buffer + kernel_data.film.pass_ray_bounces,
                            L->debug_data.num_ray_bounces);
  }
}
#endif /* __KERNEL_DEBUG__ */

#ifdef __KERNEL_CPU__
#  define WRITE_ID_SLOT(buffer, depth, id, matte_weight, name) \
    kernel_write_id_pass_cpu(buffer, depth * 2, id, matte_weight, kg->coverage_##name)
ccl_device_inline size_t kernel_write_id_pass_cpu(
    float *buffer, size_t depth, float id, float matte_weight, CoverageMap *map)
{
  if (map) {
    (*map)[id] += matte_weight;
    return 0;
  }
#else /* __KERNEL_CPU__ */
#  define WRITE_ID_SLOT(buffer, depth, id, matte_weight, name) \
    kernel_write_id_slots_gpu(buffer, depth * 2, id, matte_weight)
ccl_device_inline size_t kernel_write_id_slots_gpu(ccl_global float *buffer,
                                                   size_t depth,
                                                   float id,
                                                   float matte_weight)
{
#endif /* __KERNEL_CPU__ */
  kernel_write_id_slots(buffer, depth, id, matte_weight);
  return depth * 2;
}

ccl_device_inline void kernel_write_data_passes(KernelGlobals *kg,
                                                ccl_global float *buffer,
                                                PathRadiance *L,
                                                ShaderData *sd,
                                                ccl_addr_space PathState *state,
                                                float3 throughput)
{
#ifdef __PASSES__
  int path_flag = state->flag;

  if (!(path_flag & PATH_RAY_CAMERA))
    return;

  int flag = kernel_data.film.pass_flag;
  int light_flag = kernel_data.film.light_pass_flag;

  if (!((flag | light_flag) & PASS_ANY))
    return;

  if (!(path_flag & PATH_RAY_SINGLE_PASS_DONE)) {
    if (!(sd->flag & SD_TRANSPARENT) || kernel_data.film.pass_alpha_threshold == 0.0f ||
        average(shader_bsdf_alpha(kg, sd)) >= kernel_data.film.pass_alpha_threshold) {
      if (state->sample == 0) {
        if (flag & PASSMASK(DEPTH)) {
          float depth = camera_distance(kg, sd->P);
          kernel_write_pass_float(buffer + kernel_data.film.pass_depth, depth);
        }
        if (flag & PASSMASK(OBJECT_ID)) {
          float id = object_pass_id(kg, sd->object);
          kernel_write_pass_float(buffer + kernel_data.film.pass_object_id, id);
        }
        if (flag & PASSMASK(MATERIAL_ID)) {
          float id = shader_pass_id(kg, sd);
          kernel_write_pass_float(buffer + kernel_data.film.pass_material_id, id);
        }
      }

      if (flag & PASSMASK(NORMAL)) {
        float3 normal = shader_bsdf_average_normal(kg, sd);
        kernel_write_pass_float3(buffer + kernel_data.film.pass_normal, normal);
      }
      if (flag & PASSMASK(UV)) {
        float3 uv = primitive_uv(kg, sd);
        kernel_write_pass_float3(buffer + kernel_data.film.pass_uv, uv);
      }
      if (flag & PASSMASK(MOTION)) {
        float4 speed = primitive_motion_vector(kg, sd);
        kernel_write_pass_float4(buffer + kernel_data.film.pass_motion, speed);
        kernel_write_pass_float(buffer + kernel_data.film.pass_motion_weight, 1.0f);
      }

      state->flag |= PATH_RAY_SINGLE_PASS_DONE;
    }
  }

  if (kernel_data.film.cryptomatte_passes) {
    const float matte_weight = average(throughput) *
                               (1.0f - average(shader_bsdf_transparency(kg, sd)));
    if (matte_weight > 0.0f) {
      ccl_global float *cryptomatte_buffer = buffer + kernel_data.film.pass_cryptomatte;
      if (kernel_data.film.cryptomatte_passes & CRYPT_OBJECT) {
        float id = object_cryptomatte_id(kg, sd->object);
        cryptomatte_buffer += WRITE_ID_SLOT(
            cryptomatte_buffer, kernel_data.film.cryptomatte_depth, id, matte_weight, object);
      }
      if (kernel_data.film.cryptomatte_passes & CRYPT_MATERIAL) {
        float id = shader_cryptomatte_id(kg, sd->shader);
        cryptomatte_buffer += WRITE_ID_SLOT(
            cryptomatte_buffer, kernel_data.film.cryptomatte_depth, id, matte_weight, material);
      }
      if (kernel_data.film.cryptomatte_passes & CRYPT_ASSET) {
        float id = object_cryptomatte_asset_id(kg, sd->object);
        cryptomatte_buffer += WRITE_ID_SLOT(
            cryptomatte_buffer, kernel_data.film.cryptomatte_depth, id, matte_weight, asset);
      }
    }
  }

  if (light_flag & PASSMASK_COMPONENT(DIFFUSE))
    L->color_diffuse += shader_bsdf_diffuse(kg, sd) * throughput;
  if (light_flag & PASSMASK_COMPONENT(GLOSSY))
    L->color_glossy += shader_bsdf_glossy(kg, sd) * throughput;
  if (light_flag & PASSMASK_COMPONENT(TRANSMISSION))
    L->color_transmission += shader_bsdf_transmission(kg, sd) * throughput;
  if (light_flag & PASSMASK_COMPONENT(SUBSURFACE))
    L->color_subsurface += shader_bsdf_subsurface(kg, sd) * throughput;

  if (light_flag & PASSMASK(MIST)) {
    /* bring depth into 0..1 range */
    float mist_start = kernel_data.film.mist_start;
    float mist_inv_depth = kernel_data.film.mist_inv_depth;

    float depth = camera_distance(kg, sd->P);
    float mist = saturate((depth - mist_start) * mist_inv_depth);

    /* falloff */
    float mist_falloff = kernel_data.film.mist_falloff;

    if (mist_falloff == 1.0f)
      ;
    else if (mist_falloff == 2.0f)
      mist = mist * mist;
    else if (mist_falloff == 0.5f)
      mist = sqrtf(mist);
    else
      mist = powf(mist, mist_falloff);

    /* modulate by transparency */
    float3 alpha = shader_bsdf_alpha(kg, sd);
    L->mist += (1.0f - mist) * average(throughput * alpha);
  }
#endif
}

ccl_device_inline void kernel_write_light_passes(KernelGlobals *kg,
                                                 ccl_global float *buffer,
                                                 PathRadiance *L)
{
#ifdef __PASSES__
  int light_flag = kernel_data.film.light_pass_flag;

  if (!kernel_data.film.use_light_pass)
    return;

  if (light_flag & PASSMASK(DIFFUSE_INDIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_diffuse_indirect, L->indirect_diffuse);
  if (light_flag & PASSMASK(GLOSSY_INDIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_glossy_indirect, L->indirect_glossy);
  if (light_flag & PASSMASK(TRANSMISSION_INDIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_transmission_indirect,
                             L->indirect_transmission);
  if (light_flag & PASSMASK(SUBSURFACE_INDIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_subsurface_indirect,
                             L->indirect_subsurface);
  if (light_flag & PASSMASK(VOLUME_INDIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_volume_indirect, L->indirect_scatter);
  if (light_flag & PASSMASK(DIFFUSE_DIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_diffuse_direct, L->direct_diffuse);
  if (light_flag & PASSMASK(GLOSSY_DIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_glossy_direct, L->direct_glossy);
  if (light_flag & PASSMASK(TRANSMISSION_DIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_transmission_direct,
                             L->direct_transmission);
  if (light_flag & PASSMASK(SUBSURFACE_DIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_subsurface_direct,
                             L->direct_subsurface);
  if (light_flag & PASSMASK(VOLUME_DIRECT))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_volume_direct, L->direct_scatter);

  if (light_flag & PASSMASK(EMISSION))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_emission, L->emission);
  if (light_flag & PASSMASK(BACKGROUND))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_background, L->background);
  if (light_flag & PASSMASK(AO))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_ao, L->ao);

  if (light_flag & PASSMASK(DIFFUSE_COLOR))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_diffuse_color, L->color_diffuse);
  if (light_flag & PASSMASK(GLOSSY_COLOR))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_glossy_color, L->color_glossy);
  if (light_flag & PASSMASK(TRANSMISSION_COLOR))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_transmission_color,
                             L->color_transmission);
  if (light_flag & PASSMASK(SUBSURFACE_COLOR))
    kernel_write_pass_float3(buffer + kernel_data.film.pass_subsurface_color, L->color_subsurface);
  if (light_flag & PASSMASK(SHADOW)) {
    float4 shadow = L->shadow;
    shadow.w = kernel_data.film.pass_shadow_scale;
    kernel_write_pass_float4(buffer + kernel_data.film.pass_shadow, shadow);
  }
  if (light_flag & PASSMASK(MIST))
    kernel_write_pass_float(buffer + kernel_data.film.pass_mist, 1.0f - L->mist);
#endif
}

ccl_device_inline void kernel_write_result(KernelGlobals *kg,
                                           ccl_global float *buffer,
                                           int sample,
                                           PathRadiance *L)
{
  PROFILING_INIT(kg, PROFILING_WRITE_RESULT);
  PROFILING_OBJECT(PRIM_NONE);

  float alpha;
  float3 L_sum = path_radiance_clamp_and_sum(kg, L, &alpha);

  kernel_write_pass_float4(buffer, make_float4(L_sum.x, L_sum.y, L_sum.z, alpha));

  kernel_write_light_passes(kg, buffer, L);

#ifdef __DENOISING_FEATURES__
  if (kernel_data.film.pass_denoising_data) {
#  ifdef __SHADOW_TRICKS__
    kernel_write_denoising_shadow(kg,
                                  buffer + kernel_data.film.pass_denoising_data,
                                  sample,
                                  average(L->path_total),
                                  average(L->path_total_shaded));
#  else
    kernel_write_denoising_shadow(
        kg, buffer + kernel_data.film.pass_denoising_data, sample, 0.0f, 0.0f);
#  endif
    if (kernel_data.film.pass_denoising_clean) {
      float3 noisy, clean;
      path_radiance_split_denoising(kg, L, &noisy, &clean);
      kernel_write_pass_float3_variance(
          buffer + kernel_data.film.pass_denoising_data + DENOISING_PASS_COLOR, noisy);
      kernel_write_pass_float3_unaligned(buffer + kernel_data.film.pass_denoising_clean, clean);
    }
    else {
      kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data +
                                            DENOISING_PASS_COLOR,
                                        ensure_finite3(L_sum));
    }

    kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data +
                                          DENOISING_PASS_NORMAL,
                                      L->denoising_normal);
    kernel_write_pass_float3_variance(buffer + kernel_data.film.pass_denoising_data +
                                          DENOISING_PASS_ALBEDO,
                                      L->denoising_albedo);
    kernel_write_pass_float_variance(
        buffer + kernel_data.film.pass_denoising_data + DENOISING_PASS_DEPTH, L->denoising_depth);
  }
#endif /* __DENOISING_FEATURES__ */

#ifdef __KERNEL_DEBUG__
  kernel_write_debug_passes(kg, buffer, L);
#endif
}

CCL_NAMESPACE_END
