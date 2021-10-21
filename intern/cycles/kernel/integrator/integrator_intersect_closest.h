/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

#include "kernel/kernel_differential.h"
#include "kernel/kernel_light.h"
#include "kernel/kernel_path_state.h"
#include "kernel/kernel_projection.h"
#include "kernel/kernel_shadow_catcher.h"

#include "kernel/geom/geom.h"

#include "kernel/bvh/bvh.h"

CCL_NAMESPACE_BEGIN

template<uint32_t current_kernel>
ccl_device_forceinline bool integrator_intersect_terminate(KernelGlobals kg,
                                                           IntegratorState state,
                                                           const int shader_flags)
{

  /* Optional AO bounce termination.
   * We continue evaluating emissive/transparent surfaces and volumes, similar
   * to direct lighting. Only if we know there are none can we terminate the
   * path immediately. */
  if (path_state_ao_bounce(kg, state)) {
    if (shader_flags & (SD_HAS_TRANSPARENT_SHADOW | SD_HAS_EMISSION)) {
      INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
    }
    else if (!integrator_state_volume_stack_is_empty(kg, state)) {
      INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_TERMINATE_AFTER_VOLUME;
    }
    else {
      return true;
    }
  }

  /* Load random number state. */
  RNGState rng_state;
  path_state_rng_load(state, &rng_state);

  /* We perform path termination in this kernel to avoid launching shade_surface
   * and evaluating the shader when not needed. Only for emission and transparent
   * surfaces in front of emission do we need to evaluate the shader, since we
   * perform MIS as part of indirect rays. */
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const float probability = path_state_continuation_probability(kg, state, path_flag);

  if (probability != 1.0f) {
    const float terminate = path_state_rng_1D(kg, &rng_state, PRNG_TERMINATE);

    if (probability == 0.0f || terminate >= probability) {
      if (shader_flags & SD_HAS_EMISSION) {
        /* Mark path to be terminated right after shader evaluation on the surface. */
        INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_TERMINATE_ON_NEXT_SURFACE;
      }
      else if (!integrator_state_volume_stack_is_empty(kg, state)) {
        /* TODO: only do this for emissive volumes. */
        INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_TERMINATE_IN_NEXT_VOLUME;
      }
      else {
        return true;
      }
    }
  }

  return false;
}

/* Note that current_kernel is a template value since making this a variable
 * leads to poor performance with CUDA atomics. */
template<uint32_t current_kernel>
ccl_device_forceinline void integrator_intersect_shader_next_kernel(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private const Intersection *ccl_restrict isect,
    const int shader,
    const int shader_flags)
{
  /* Note on scheduling.
   *
   * When there is no shadow catcher split the scheduling is simple: schedule surface shading with
   * or without raytrace support, depending on the shader used.
   *
   * When there is a shadow catcher split the general idea is to have the following configuration:
   *
   *  - Schedule surface shading kernel (with corresponding raytrace support) for the ray which
   *    will trace shadow catcher object.
   *
   *  - When no alpha-over of approximate shadow catcher is needed, schedule surface shading for
   *    the matte ray.
   *
   *  - Otherwise schedule background shading kernel, so that we have a background to alpha-over
   *    on. The background kernel will then schedule surface shading for the matte ray.
   *
   * Note that the splitting leaves kernel and sorting counters as-is, so use INIT semantic for
   * the matte path. */

  const bool use_raytrace_kernel = (shader_flags & SD_HAS_RAYTRACE);

  if (use_raytrace_kernel) {
    INTEGRATOR_PATH_NEXT_SORTED(
        current_kernel, DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE, shader);
  }
  else {
    INTEGRATOR_PATH_NEXT_SORTED(current_kernel, DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE, shader);
  }

#ifdef __SHADOW_CATCHER__
  const int object_flags = intersection_get_object_flags(kg, isect);
  if (kernel_shadow_catcher_split(kg, state, object_flags)) {
    if (kernel_data.film.pass_background != PASS_UNUSED && !kernel_data.background.transparent) {
      INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_SHADOW_CATCHER_BACKGROUND;

      INTEGRATOR_PATH_INIT(DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
    }
    else if (use_raytrace_kernel) {
      INTEGRATOR_PATH_INIT_SORTED(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE, shader);
    }
    else {
      INTEGRATOR_PATH_INIT_SORTED(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE, shader);
    }
  }
#endif
}

ccl_device void integrator_intersect_closest(KernelGlobals kg, IntegratorState state)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_CLOSEST);

  /* Read ray from integrator state into local memory. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_ray(kg, state, &ray);
  kernel_assert(ray.t != 0.0f);

  const uint visibility = path_state_ray_visibility(state);
  const int last_isect_prim = INTEGRATOR_STATE(state, isect, prim);
  const int last_isect_object = INTEGRATOR_STATE(state, isect, object);

  /* Trick to use short AO rays to approximate indirect light at the end of the path. */
  if (path_state_ao_bounce(kg, state)) {
    ray.t = kernel_data.integrator.ao_bounces_distance;

    const float object_ao_distance = kernel_tex_fetch(__objects, last_isect_object).ao_distance;
    if (object_ao_distance != 0.0f) {
      ray.t = object_ao_distance;
    }
  }

  /* Scene Intersection. */
  Intersection isect ccl_optional_struct_init;
  bool hit = scene_intersect(kg, &ray, visibility, &isect);

  /* TODO: remove this and do it in the various intersection functions instead. */
  if (!hit) {
    isect.prim = PRIM_NONE;
  }

  /* Light intersection for MIS. */
  if (kernel_data.integrator.use_lamp_mis) {
    /* NOTE: if we make lights visible to camera rays, we'll need to initialize
     * these in the path_state_init. */
    const int last_type = INTEGRATOR_STATE(state, isect, type);
    const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

    hit = lights_intersect(
              kg, &ray, &isect, last_isect_prim, last_isect_object, last_type, path_flag) ||
          hit;
  }

  /* Write intersection result into global integrator state memory. */
  integrator_state_write_isect(kg, state, &isect);

#ifdef __VOLUME__
  if (!integrator_state_volume_stack_is_empty(kg, state)) {
    const bool hit_surface = hit && !(isect.type & PRIMITIVE_LAMP);
    const int shader = (hit_surface) ? intersection_get_shader(kg, &isect) : SHADER_NONE;
    const int flags = (hit_surface) ? kernel_tex_fetch(__shaders, shader).flags : 0;

    if (!integrator_intersect_terminate<DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST>(
            kg, state, flags)) {
      /* Continue with volume kernel if we are inside a volume, regardless
       * if we hit anything. */
      INTEGRATOR_PATH_NEXT(DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST,
                           DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME);
    }
    else {
      INTEGRATOR_PATH_TERMINATE(DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
    }
    return;
  }
#endif

  if (hit) {
    /* Hit a surface, continue with light or surface kernel. */
    if (isect.type & PRIMITIVE_LAMP) {
      INTEGRATOR_PATH_NEXT(DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST,
                           DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT);
      return;
    }
    else {
      /* Hit a surface, continue with surface kernel unless terminated. */
      const int shader = intersection_get_shader(kg, &isect);
      const int flags = kernel_tex_fetch(__shaders, shader).flags;

      if (!integrator_intersect_terminate<DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST>(
              kg, state, flags)) {
        integrator_intersect_shader_next_kernel<DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST>(
            kg, state, &isect, shader, flags);
        return;
      }
      else {
        INTEGRATOR_PATH_TERMINATE(DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
        return;
      }
    }
  }
  else {
    /* Nothing hit, continue with background kernel. */
    INTEGRATOR_PATH_NEXT(DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST,
                         DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
    return;
  }
}

CCL_NAMESPACE_END
