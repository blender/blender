/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/bvh/bvh.h"
#include "kernel/geom/geom.h"
#include "kernel/integrator/volume_stack.h"

CCL_NAMESPACE_BEGIN

ccl_device void integrator_volume_stack_update_for_subsurface(KernelGlobals kg,
                                                              IntegratorState state,
                                                              const float3 from_P,
                                                              const float3 to_P)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_VOLUME_STACK);

  ShaderDataTinyStorage stack_sd_storage;
  ccl_private ShaderData *stack_sd = AS_SHADER_DATA(&stack_sd_storage);

  kernel_assert(kernel_data.integrator.use_volumes);

  Ray volume_ray ccl_optional_struct_init;
  volume_ray.P = from_P;
  volume_ray.D = normalize_len(to_P - from_P, &volume_ray.tmax);
  volume_ray.tmin = 0.0f;
  volume_ray.self.object = INTEGRATOR_STATE(state, isect, object);
  volume_ray.self.prim = INTEGRATOR_STATE(state, isect, prim);
  volume_ray.self.light_object = OBJECT_NONE;
  volume_ray.self.light_prim = PRIM_NONE;
  volume_ray.self.light = LAMP_NONE;
  /* Store to avoid global fetches on every intersection step. */
  const uint volume_stack_size = kernel_data.volume_stack_size;

  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const uint32_t visibility = SHADOW_CATCHER_PATH_VISIBILITY(path_flag, PATH_RAY_ALL_VISIBILITY);

#ifdef __VOLUME_RECORD_ALL__
  Intersection hits[2 * MAX_VOLUME_STACK_SIZE + 1];
  uint num_hits = scene_intersect_volume(kg, &volume_ray, hits, 2 * volume_stack_size, visibility);
  if (num_hits > 0) {
    Intersection *isect = hits;

    qsort(hits, num_hits, sizeof(Intersection), intersections_compare);

    for (uint hit = 0; hit < num_hits; ++hit, ++isect) {
      shader_setup_from_ray(kg, stack_sd, &volume_ray, isect);
      volume_stack_enter_exit(kg, state, stack_sd);
    }
  }
#else
  Intersection isect;
  int step = 0;
  while (step < 2 * volume_stack_size &&
         scene_intersect_volume(kg, &volume_ray, &isect, visibility)) {
    shader_setup_from_ray(kg, stack_sd, &volume_ray, &isect);
    volume_stack_enter_exit(kg, state, stack_sd);

    /* Move ray forward. */
    volume_ray.tmin = intersection_t_offset(isect.t);
    volume_ray.self.object = isect.object;
    volume_ray.self.prim = isect.prim;
    ++step;
  }
#endif
}

ccl_device void integrator_volume_stack_init(KernelGlobals kg, IntegratorState state)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_VOLUME_STACK);

  ShaderDataTinyStorage stack_sd_storage;
  ccl_private ShaderData *stack_sd = AS_SHADER_DATA(&stack_sd_storage);

  Ray volume_ray ccl_optional_struct_init;
  integrator_state_read_ray(state, &volume_ray);

  /* Trace ray in random direction. Any direction works, Z up is a guess to get the
   * fewest hits. */
  volume_ray.D = make_float3(0.0f, 0.0f, 1.0f);
  volume_ray.tmin = 0.0f;
  volume_ray.tmax = FLT_MAX;
  volume_ray.self.object = OBJECT_NONE;
  volume_ray.self.prim = PRIM_NONE;
  volume_ray.self.light_object = OBJECT_NONE;
  volume_ray.self.light_prim = PRIM_NONE;
  volume_ray.self.light = LAMP_NONE;

  int stack_index = 0, enclosed_index = 0;

  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const uint32_t visibility = SHADOW_CATCHER_PATH_VISIBILITY(path_flag, PATH_RAY_CAMERA);

  /* Initialize volume stack with background volume For shadow catcher the
   * background volume is always assumed to be CG. */
  if (kernel_data.background.volume_shader != SHADER_NONE) {
    if (!(path_flag & PATH_RAY_SHADOW_CATCHER_PASS)) {
      INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, stack_index, object) = OBJECT_NONE;
      INTEGRATOR_STATE_ARRAY_WRITE(
          state, volume_stack, stack_index, shader) = kernel_data.background.volume_shader;
      stack_index++;
    }
  }

  /* Store to avoid global fetches on every intersection step. */
  const uint volume_stack_size = kernel_data.volume_stack_size;

#ifdef __VOLUME_RECORD_ALL__
  Intersection hits[2 * MAX_VOLUME_STACK_SIZE + 1];
  uint num_hits = scene_intersect_volume(kg, &volume_ray, hits, 2 * volume_stack_size, visibility);
  if (num_hits > 0) {
    int enclosed_volumes[MAX_VOLUME_STACK_SIZE];
    Intersection *isect = hits;

    qsort(hits, num_hits, sizeof(Intersection), intersections_compare);

    for (uint hit = 0; hit < num_hits; ++hit, ++isect) {
      shader_setup_from_ray(kg, stack_sd, &volume_ray, isect);
      if (stack_sd->flag & SD_BACKFACING) {
        bool need_add = true;
        for (int i = 0; i < enclosed_index && need_add; ++i) {
          /* If ray exited the volume and never entered to that volume
           * it means that camera is inside such a volume.
           */
          if (enclosed_volumes[i] == stack_sd->object) {
            need_add = false;
          }
        }
        for (int i = 0; i < stack_index && need_add; ++i) {
          /* Don't add intersections twice. */
          VolumeStack entry = integrator_state_read_volume_stack(state, i);
          if (entry.object == stack_sd->object) {
            need_add = false;
            break;
          }
        }
        if (need_add && stack_index < volume_stack_size - 1) {
          const VolumeStack new_entry = {stack_sd->object, stack_sd->shader};
          integrator_state_write_volume_stack(state, stack_index, new_entry);
          ++stack_index;
        }
      }
      else {
        /* If ray from camera enters the volume, this volume shouldn't
         * be added to the stack on exit.
         */
        enclosed_volumes[enclosed_index++] = stack_sd->object;
      }
    }
  }
#else
  /* CUDA does not support definition of a variable size arrays, so use the maximum possible. */
  int enclosed_volumes[MAX_VOLUME_STACK_SIZE];
  int step = 0;

  while (stack_index < volume_stack_size - 1 && enclosed_index < MAX_VOLUME_STACK_SIZE - 1 &&
         step < 2 * volume_stack_size)
  {
    Intersection isect;
    if (!scene_intersect_volume(kg, &volume_ray, &isect, visibility)) {
      break;
    }

    shader_setup_from_ray(kg, stack_sd, &volume_ray, &isect);
    if (stack_sd->flag & SD_BACKFACING) {
      /* If ray exited the volume and never entered to that volume
       * it means that camera is inside such a volume.
       */
      bool need_add = true;
      for (int i = 0; i < enclosed_index && need_add; ++i) {
        /* If ray exited the volume and never entered to that volume
         * it means that camera is inside such a volume.
         */
        if (enclosed_volumes[i] == stack_sd->object) {
          need_add = false;
        }
      }
      for (int i = 0; i < stack_index && need_add; ++i) {
        /* Don't add intersections twice. */
        VolumeStack entry = integrator_state_read_volume_stack(state, i);
        if (entry.object == stack_sd->object) {
          need_add = false;
          break;
        }
      }
      if (need_add) {
        const VolumeStack new_entry = {stack_sd->object, stack_sd->shader};
        integrator_state_write_volume_stack(state, stack_index, new_entry);
        ++stack_index;
      }
    }
    else {
      /* If ray from camera enters the volume, this volume shouldn't
       * be added to the stack on exit.
       */
      enclosed_volumes[enclosed_index++] = stack_sd->object;
    }

    /* Move ray forward. */
    volume_ray.tmin = intersection_t_offset(isect.t);
    volume_ray.self.object = isect.object;
    volume_ray.self.prim = isect.prim;
    ++step;
  }
#endif

  /* Write terminator. */
  const VolumeStack new_entry = {OBJECT_NONE, SHADER_NONE};
  integrator_state_write_volume_stack(state, stack_index, new_entry);
}

ccl_device void integrator_intersect_volume_stack(KernelGlobals kg, IntegratorState state)
{
  integrator_volume_stack_init(kg, state);

  if (INTEGRATOR_STATE(state, path, flag) & PATH_RAY_SHADOW_CATCHER_PASS) {
    /* Volume stack re-init for shadow catcher, continue with shading of hit. */
    integrator_intersect_next_kernel_after_shadow_catcher_volume<
        DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK>(kg, state);
  }
  else {
    /* Volume stack init for camera rays, continue with intersection of camera ray. */
    integrator_path_next(kg,
                         state,
                         DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK,
                         DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
  }
}

CCL_NAMESPACE_END
