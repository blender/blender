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

#ifdef __OSL__
#  include "kernel/osl/osl_shader.h"
#endif

#include "kernel/kernel_random.h"
#include "kernel/kernel_projection.h"
#include "kernel/kernel_montecarlo.h"
#include "kernel/kernel_differential.h"
#include "kernel/kernel_camera.h"

#include "kernel/geom/geom.h"
#include "kernel/bvh/bvh.h"

#include "kernel/kernel_accumulate.h"
#include "kernel/kernel_shader.h"
#include "kernel/kernel_light.h"
#include "kernel/kernel_passes.h"

#if defined(__VOLUME__) || defined(__SUBSURFACE__)
#  include "kernel/kernel_volume.h"
#endif

#ifdef __SUBSURFACE__
#  include "kernel/kernel_subsurface.h"
#endif

#include "kernel/kernel_path_state.h"
#include "kernel/kernel_shadow.h"
#include "kernel/kernel_emission.h"
#include "kernel/kernel_path_common.h"
#include "kernel/kernel_path_surface.h"
#include "kernel/kernel_path_volume.h"
#include "kernel/kernel_path_subsurface.h"

CCL_NAMESPACE_BEGIN

ccl_device_forceinline bool kernel_path_scene_intersect(KernelGlobals *kg,
                                                        ccl_addr_space PathState *state,
                                                        Ray *ray,
                                                        Intersection *isect,
                                                        PathRadiance *L)
{
  PROFILING_INIT(kg, PROFILING_SCENE_INTERSECT);

  uint visibility = path_state_ray_visibility(kg, state);

  if (path_state_ao_bounce(kg, state)) {
    visibility = PATH_RAY_SHADOW;
    ray->t = kernel_data.background.ao_distance;
  }

  bool hit = scene_intersect(kg, ray, visibility, isect);

#ifdef __KERNEL_DEBUG__
  if (state->flag & PATH_RAY_CAMERA) {
    L->debug_data.num_bvh_traversed_nodes += isect->num_traversed_nodes;
    L->debug_data.num_bvh_traversed_instances += isect->num_traversed_instances;
    L->debug_data.num_bvh_intersections += isect->num_intersections;
  }
  L->debug_data.num_ray_bounces++;
#endif /* __KERNEL_DEBUG__ */

  return hit;
}

ccl_device_forceinline void kernel_path_lamp_emission(KernelGlobals *kg,
                                                      ccl_addr_space PathState *state,
                                                      Ray *ray,
                                                      float3 throughput,
                                                      ccl_addr_space Intersection *isect,
                                                      ShaderData *emission_sd,
                                                      PathRadiance *L)
{
  PROFILING_INIT(kg, PROFILING_INDIRECT_EMISSION);

#ifdef __LAMP_MIS__
  if (kernel_data.integrator.use_lamp_mis && !(state->flag & PATH_RAY_CAMERA)) {
    /* ray starting from previous non-transparent bounce */
    Ray light_ray;

    light_ray.P = ray->P - state->ray_t * ray->D;
    state->ray_t += isect->t;
    light_ray.D = ray->D;
    light_ray.t = state->ray_t;
    light_ray.time = ray->time;
    light_ray.dD = ray->dD;
    light_ray.dP = ray->dP;

    /* intersect with lamp */
    float3 emission = make_float3(0.0f, 0.0f, 0.0f);

    if (indirect_lamp_emission(kg, emission_sd, state, &light_ray, &emission))
      path_radiance_accum_emission(L, state, throughput, emission);
  }
#endif /* __LAMP_MIS__ */
}

ccl_device_forceinline void kernel_path_background(KernelGlobals *kg,
                                                   ccl_addr_space PathState *state,
                                                   ccl_addr_space Ray *ray,
                                                   float3 throughput,
                                                   ShaderData *sd,
                                                   PathRadiance *L)
{
  /* eval background shader if nothing hit */
  if (kernel_data.background.transparent && (state->flag & PATH_RAY_TRANSPARENT_BACKGROUND)) {
    L->transparent += average(throughput);

#ifdef __PASSES__
    if (!(kernel_data.film.light_pass_flag & PASSMASK(BACKGROUND)))
#endif /* __PASSES__ */
      return;
  }

  /* When using the ao bounces approximation, adjust background
   * shader intensity with ao factor. */
  if (path_state_ao_bounce(kg, state)) {
    throughput *= kernel_data.background.ao_bounces_factor;
  }

#ifdef __BACKGROUND__
  /* sample background shader */
  float3 L_background = indirect_background(kg, sd, state, ray);
  path_radiance_accum_background(L, state, throughput, L_background);
#endif /* __BACKGROUND__ */
}

#ifndef __SPLIT_KERNEL__

#  ifdef __VOLUME__
ccl_device_forceinline VolumeIntegrateResult kernel_path_volume(KernelGlobals *kg,
                                                                ShaderData *sd,
                                                                PathState *state,
                                                                Ray *ray,
                                                                float3 *throughput,
                                                                ccl_addr_space Intersection *isect,
                                                                bool hit,
                                                                ShaderData *emission_sd,
                                                                PathRadiance *L)
{
  PROFILING_INIT(kg, PROFILING_VOLUME);

  /* Sanitize volume stack. */
  if (!hit) {
    kernel_volume_clean_stack(kg, state->volume_stack);
  }

  if (state->volume_stack[0].shader == SHADER_NONE) {
    return VOLUME_PATH_ATTENUATED;
  }

  /* volume attenuation, emission, scatter */
  Ray volume_ray = *ray;
  volume_ray.t = (hit) ? isect->t : FLT_MAX;

  bool heterogeneous = volume_stack_is_heterogeneous(kg, state->volume_stack);

#    ifdef __VOLUME_DECOUPLED__
  int sampling_method = volume_stack_sampling_method(kg, state->volume_stack);
  bool direct = (state->flag & PATH_RAY_CAMERA) != 0;
  bool decoupled = kernel_volume_use_decoupled(kg, heterogeneous, direct, sampling_method);

  if (decoupled) {
    /* cache steps along volume for repeated sampling */
    VolumeSegment volume_segment;

    shader_setup_from_volume(kg, sd, &volume_ray);
    kernel_volume_decoupled_record(kg, state, &volume_ray, sd, &volume_segment, heterogeneous);

    volume_segment.sampling_method = sampling_method;

    /* emission */
    if (volume_segment.closure_flag & SD_EMISSION)
      path_radiance_accum_emission(L, state, *throughput, volume_segment.accum_emission);

    /* scattering */
    VolumeIntegrateResult result = VOLUME_PATH_ATTENUATED;

    if (volume_segment.closure_flag & SD_SCATTER) {
      int all = kernel_data.integrator.sample_all_lights_indirect;

      /* direct light sampling */
      kernel_branched_path_volume_connect_light(
          kg, sd, emission_sd, *throughput, state, L, all, &volume_ray, &volume_segment);

      /* indirect sample. if we use distance sampling and take just
       * one sample for direct and indirect light, we could share
       * this computation, but makes code a bit complex */
      float rphase = path_state_rng_1D(kg, state, PRNG_PHASE_CHANNEL);
      float rscatter = path_state_rng_1D(kg, state, PRNG_SCATTER_DISTANCE);

      result = kernel_volume_decoupled_scatter(
          kg, state, &volume_ray, sd, throughput, rphase, rscatter, &volume_segment, NULL, true);
    }

    /* free cached steps */
    kernel_volume_decoupled_free(kg, &volume_segment);

    if (result == VOLUME_PATH_SCATTERED) {
      if (kernel_path_volume_bounce(kg, sd, throughput, state, &L->state, ray))
        return VOLUME_PATH_SCATTERED;
      else
        return VOLUME_PATH_MISSED;
    }
    else {
      *throughput *= volume_segment.accum_transmittance;
    }
  }
  else
#    endif /* __VOLUME_DECOUPLED__ */
  {
    /* integrate along volume segment with distance sampling */
    VolumeIntegrateResult result = kernel_volume_integrate(
        kg, state, sd, &volume_ray, L, throughput, heterogeneous);

#    ifdef __VOLUME_SCATTER__
    if (result == VOLUME_PATH_SCATTERED) {
      /* direct lighting */
      kernel_path_volume_connect_light(kg, sd, emission_sd, *throughput, state, L);

      /* indirect light bounce */
      if (kernel_path_volume_bounce(kg, sd, throughput, state, &L->state, ray))
        return VOLUME_PATH_SCATTERED;
      else
        return VOLUME_PATH_MISSED;
    }
#    endif /* __VOLUME_SCATTER__ */
  }

  return VOLUME_PATH_ATTENUATED;
}
#  endif /* __VOLUME__ */

#endif /* __SPLIT_KERNEL__ */

ccl_device_forceinline bool kernel_path_shader_apply(KernelGlobals *kg,
                                                     ShaderData *sd,
                                                     ccl_addr_space PathState *state,
                                                     ccl_addr_space Ray *ray,
                                                     float3 throughput,
                                                     ShaderData *emission_sd,
                                                     PathRadiance *L,
                                                     ccl_global float *buffer)
{
  PROFILING_INIT(kg, PROFILING_SHADER_APPLY);

#ifdef __SHADOW_TRICKS__
  if ((sd->object_flag & SD_OBJECT_SHADOW_CATCHER)) {
    if (state->flag & PATH_RAY_TRANSPARENT_BACKGROUND) {
      state->flag |= (PATH_RAY_SHADOW_CATCHER | PATH_RAY_STORE_SHADOW_INFO);

      float3 bg = make_float3(0.0f, 0.0f, 0.0f);
      if (!kernel_data.background.transparent) {
        bg = indirect_background(kg, emission_sd, state, ray);
      }
      path_radiance_accum_shadowcatcher(L, throughput, bg);
    }
  }
  else if (state->flag & PATH_RAY_SHADOW_CATCHER) {
    /* Only update transparency after shadow catcher bounce. */
    L->shadow_transparency *= average(shader_bsdf_transparency(kg, sd));
  }
#endif /* __SHADOW_TRICKS__ */

  /* holdout */
#ifdef __HOLDOUT__
  if (((sd->flag & SD_HOLDOUT) || (sd->object_flag & SD_OBJECT_HOLDOUT_MASK)) &&
      (state->flag & PATH_RAY_TRANSPARENT_BACKGROUND)) {
    if (kernel_data.background.transparent) {
      float3 holdout_weight;
      if (sd->object_flag & SD_OBJECT_HOLDOUT_MASK) {
        holdout_weight = make_float3(1.0f, 1.0f, 1.0f);
      }
      else {
        holdout_weight = shader_holdout_eval(kg, sd);
      }
      /* any throughput is ok, should all be identical here */
      L->transparent += average(holdout_weight * throughput);
    }

    if (sd->object_flag & SD_OBJECT_HOLDOUT_MASK) {
      return false;
    }
  }
#endif /* __HOLDOUT__ */

  /* holdout mask objects do not write data passes */
  kernel_write_data_passes(kg, buffer, L, sd, state, throughput);

  /* blurring of bsdf after bounces, for rays that have a small likelihood
   * of following this particular path (diffuse, rough glossy) */
  if (kernel_data.integrator.filter_glossy != FLT_MAX) {
    float blur_pdf = kernel_data.integrator.filter_glossy * state->min_ray_pdf;

    if (blur_pdf < 1.0f) {
      float blur_roughness = sqrtf(1.0f - blur_pdf) * 0.5f;
      shader_bsdf_blur(kg, sd, blur_roughness);
    }
  }

#ifdef __EMISSION__
  /* emission */
  if (sd->flag & SD_EMISSION) {
    float3 emission = indirect_primitive_emission(
        kg, sd, sd->ray_length, state->flag, state->ray_pdf);
    path_radiance_accum_emission(L, state, throughput, emission);
  }
#endif /* __EMISSION__ */

  return true;
}

ccl_device_noinline void kernel_path_ao(KernelGlobals *kg,
                                        ShaderData *sd,
                                        ShaderData *emission_sd,
                                        PathRadiance *L,
                                        ccl_addr_space PathState *state,
                                        float3 throughput,
                                        float3 ao_alpha)
{
  PROFILING_INIT(kg, PROFILING_AO);

  /* todo: solve correlation */
  float bsdf_u, bsdf_v;

  path_state_rng_2D(kg, state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

  float ao_factor = kernel_data.background.ao_factor;
  float3 ao_N;
  float3 ao_bsdf = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
  float3 ao_D;
  float ao_pdf;

  sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

  if (dot(sd->Ng, ao_D) > 0.0f && ao_pdf != 0.0f) {
    Ray light_ray;
    float3 ao_shadow;

    light_ray.P = ray_offset(sd->P, sd->Ng);
    light_ray.D = ao_D;
    light_ray.t = kernel_data.background.ao_distance;
    light_ray.time = sd->time;
    light_ray.dP = sd->dP;
    light_ray.dD = differential3_zero();

    if (!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &ao_shadow)) {
      path_radiance_accum_ao(L, state, throughput, ao_alpha, ao_bsdf, ao_shadow);
    }
    else {
      path_radiance_accum_total_ao(L, state, throughput, ao_bsdf);
    }
  }
}

#ifndef __SPLIT_KERNEL__

#  if defined(__BRANCHED_PATH__) || defined(__BAKING__)

ccl_device void kernel_path_indirect(KernelGlobals *kg,
                                     ShaderData *sd,
                                     ShaderData *emission_sd,
                                     Ray *ray,
                                     float3 throughput,
                                     PathState *state,
                                     PathRadiance *L)
{
#    ifdef __SUBSURFACE__
  SubsurfaceIndirectRays ss_indirect;
  kernel_path_subsurface_init_indirect(&ss_indirect);

  for (;;) {
#    endif /* __SUBSURFACE__ */

    /* path iteration */
    for (;;) {
      /* Find intersection with objects in scene. */
      Intersection isect;
      bool hit = kernel_path_scene_intersect(kg, state, ray, &isect, L);

      /* Find intersection with lamps and compute emission for MIS. */
      kernel_path_lamp_emission(kg, state, ray, throughput, &isect, sd, L);

#    ifdef __VOLUME__
      /* Volume integration. */
      VolumeIntegrateResult result = kernel_path_volume(
          kg, sd, state, ray, &throughput, &isect, hit, emission_sd, L);

      if (result == VOLUME_PATH_SCATTERED) {
        continue;
      }
      else if (result == VOLUME_PATH_MISSED) {
        break;
      }
#    endif /* __VOLUME__*/

      /* Shade background. */
      if (!hit) {
        kernel_path_background(kg, state, ray, throughput, sd, L);
        break;
      }
      else if (path_state_ao_bounce(kg, state)) {
        break;
      }

      /* Setup shader data. */
      shader_setup_from_ray(kg, sd, &isect, ray);

      /* Skip most work for volume bounding surface. */
#    ifdef __VOLUME__
      if (!(sd->flag & SD_HAS_ONLY_VOLUME)) {
#    endif

        /* Evaluate shader. */
        shader_eval_surface(kg, sd, state, state->flag);
        shader_prepare_closures(sd, state);

        /* Apply shadow catcher, holdout, emission. */
        if (!kernel_path_shader_apply(kg, sd, state, ray, throughput, emission_sd, L, NULL)) {
          break;
        }

        /* path termination. this is a strange place to put the termination, it's
         * mainly due to the mixed in MIS that we use. gives too many unneeded
         * shader evaluations, only need emission if we are going to terminate */
        float probability = path_state_continuation_probability(kg, state, throughput);

        if (probability == 0.0f) {
          break;
        }
        else if (probability != 1.0f) {
          float terminate = path_state_rng_1D(kg, state, PRNG_TERMINATE);

          if (terminate >= probability)
            break;

          throughput /= probability;
        }

        kernel_update_denoising_features(kg, sd, state, L);

#    ifdef __AO__
        /* ambient occlusion */
        if (kernel_data.integrator.use_ambient_occlusion) {
          kernel_path_ao(kg, sd, emission_sd, L, state, throughput, make_float3(0.0f, 0.0f, 0.0f));
        }
#    endif /* __AO__ */

#    ifdef __SUBSURFACE__
        /* bssrdf scatter to a different location on the same object, replacing
         * the closures with a diffuse BSDF */
        if (sd->flag & SD_BSSRDF) {
          if (kernel_path_subsurface_scatter(
                  kg, sd, emission_sd, L, state, ray, &throughput, &ss_indirect)) {
            break;
          }
        }
#    endif /* __SUBSURFACE__ */

#    if defined(__EMISSION__)
        if (kernel_data.integrator.use_direct_light) {
          int all = (kernel_data.integrator.sample_all_lights_indirect) ||
                    (state->flag & PATH_RAY_SHADOW_CATCHER);
          kernel_branched_path_surface_connect_light(
              kg, sd, emission_sd, state, throughput, 1.0f, L, all);
        }
#    endif /* defined(__EMISSION__) */

#    ifdef __VOLUME__
      }
#    endif

      if (!kernel_path_surface_bounce(kg, sd, &throughput, state, &L->state, ray))
        break;
    }

#    ifdef __SUBSURFACE__
    /* Trace indirect subsurface rays by restarting the loop. this uses less
     * stack memory than invoking kernel_path_indirect.
     */
    if (ss_indirect.num_rays) {
      kernel_path_subsurface_setup_indirect(kg, &ss_indirect, state, ray, L, &throughput);
    }
    else {
      break;
    }
  }
#    endif /* __SUBSURFACE__ */
}

#  endif /* defined(__BRANCHED_PATH__) || defined(__BAKING__) */

ccl_device_forceinline void kernel_path_integrate(KernelGlobals *kg,
                                                  PathState *state,
                                                  float3 throughput,
                                                  Ray *ray,
                                                  PathRadiance *L,
                                                  ccl_global float *buffer,
                                                  ShaderData *emission_sd)
{
  PROFILING_INIT(kg, PROFILING_PATH_INTEGRATE);

  /* Shader data memory used for both volumes and surfaces, saves stack space. */
  ShaderData sd;

#  ifdef __SUBSURFACE__
  SubsurfaceIndirectRays ss_indirect;
  kernel_path_subsurface_init_indirect(&ss_indirect);

  for (;;) {
#  endif /* __SUBSURFACE__ */

    /* path iteration */
    for (;;) {
      /* Find intersection with objects in scene. */
      Intersection isect;
      bool hit = kernel_path_scene_intersect(kg, state, ray, &isect, L);

      /* Find intersection with lamps and compute emission for MIS. */
      kernel_path_lamp_emission(kg, state, ray, throughput, &isect, &sd, L);

#  ifdef __VOLUME__
      /* Volume integration. */
      VolumeIntegrateResult result = kernel_path_volume(
          kg, &sd, state, ray, &throughput, &isect, hit, emission_sd, L);

      if (result == VOLUME_PATH_SCATTERED) {
        continue;
      }
      else if (result == VOLUME_PATH_MISSED) {
        break;
      }
#  endif /* __VOLUME__*/

      /* Shade background. */
      if (!hit) {
        kernel_path_background(kg, state, ray, throughput, &sd, L);
        break;
      }
      else if (path_state_ao_bounce(kg, state)) {
        break;
      }

      /* Setup shader data. */
      shader_setup_from_ray(kg, &sd, &isect, ray);

      /* Skip most work for volume bounding surface. */
#  ifdef __VOLUME__
      if (!(sd.flag & SD_HAS_ONLY_VOLUME)) {
#  endif

        /* Evaluate shader. */
        shader_eval_surface(kg, &sd, state, state->flag);
        shader_prepare_closures(&sd, state);

        /* Apply shadow catcher, holdout, emission. */
        if (!kernel_path_shader_apply(kg, &sd, state, ray, throughput, emission_sd, L, buffer)) {
          break;
        }

        /* path termination. this is a strange place to put the termination, it's
         * mainly due to the mixed in MIS that we use. gives too many unneeded
         * shader evaluations, only need emission if we are going to terminate */
        float probability = path_state_continuation_probability(kg, state, throughput);

        if (probability == 0.0f) {
          break;
        }
        else if (probability != 1.0f) {
          float terminate = path_state_rng_1D(kg, state, PRNG_TERMINATE);
          if (terminate >= probability)
            break;

          throughput /= probability;
        }

#  ifdef __DENOISING_FEATURES__
        kernel_update_denoising_features(kg, &sd, state, L);
#  endif

#  ifdef __AO__
        /* ambient occlusion */
        if (kernel_data.integrator.use_ambient_occlusion) {
          kernel_path_ao(kg, &sd, emission_sd, L, state, throughput, shader_bsdf_alpha(kg, &sd));
        }
#  endif /* __AO__ */

#  ifdef __SUBSURFACE__
        /* bssrdf scatter to a different location on the same object, replacing
         * the closures with a diffuse BSDF */
        if (sd.flag & SD_BSSRDF) {
          if (kernel_path_subsurface_scatter(
                  kg, &sd, emission_sd, L, state, ray, &throughput, &ss_indirect)) {
            break;
          }
        }
#  endif /* __SUBSURFACE__ */

#  ifdef __EMISSION__
        /* direct lighting */
        kernel_path_surface_connect_light(kg, &sd, emission_sd, throughput, state, L);
#  endif /* __EMISSION__ */

#  ifdef __VOLUME__
      }
#  endif

      /* compute direct lighting and next bounce */
      if (!kernel_path_surface_bounce(kg, &sd, &throughput, state, &L->state, ray))
        break;
    }

#  ifdef __SUBSURFACE__
    /* Trace indirect subsurface rays by restarting the loop. this uses less
     * stack memory than invoking kernel_path_indirect.
     */
    if (ss_indirect.num_rays) {
      kernel_path_subsurface_setup_indirect(kg, &ss_indirect, state, ray, L, &throughput);
    }
    else {
      break;
    }
  }
#  endif /* __SUBSURFACE__ */
}

ccl_device void kernel_path_trace(
    KernelGlobals *kg, ccl_global float *buffer, int sample, int x, int y, int offset, int stride)
{
  PROFILING_INIT(kg, PROFILING_RAY_SETUP);

  /* buffer offset */
  int index = offset + x + y * stride;
  int pass_stride = kernel_data.film.pass_stride;

  buffer += index * pass_stride;

  /* Initialize random numbers and sample ray. */
  uint rng_hash;
  Ray ray;

  kernel_path_trace_setup(kg, sample, x, y, &rng_hash, &ray);

  if (ray.t == 0.0f) {
    return;
  }

  /* Initialize state. */
  float3 throughput = make_float3(1.0f, 1.0f, 1.0f);

  PathRadiance L;
  path_radiance_init(&L, kernel_data.film.use_light_pass);

  ShaderDataTinyStorage emission_sd_storage;
  ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);

  PathState state;
  path_state_init(kg, emission_sd, &state, rng_hash, sample, &ray);

  /* Integrate. */
  kernel_path_integrate(kg, &state, throughput, &ray, &L, buffer, emission_sd);

  kernel_write_result(kg, buffer, sample, &L);
}

#endif /* __SPLIT_KERNEL__ */

CCL_NAMESPACE_END
