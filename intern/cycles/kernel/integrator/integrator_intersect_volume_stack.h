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

#include "kernel/bvh/bvh.h"
#include "kernel/geom/geom.h"
#include "kernel/integrator/integrator_volume_stack.h"
#include "kernel/kernel_shader.h"

CCL_NAMESPACE_BEGIN

ccl_device void integrator_volume_stack_update_for_subsurface(INTEGRATOR_STATE_ARGS,
                                                              const float3 from_P,
                                                              const float3 to_P)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_VOLUME_STACK);

  ShaderDataTinyStorage stack_sd_storage;
  ShaderData *stack_sd = AS_SHADER_DATA(&stack_sd_storage);

  kernel_assert(kernel_data.integrator.use_volumes);

  Ray volume_ray ccl_optional_struct_init;
  volume_ray.P = from_P;
  volume_ray.D = normalize_len(to_P - from_P, &volume_ray.t);

  /* Store to avoid global fetches on every intersection step. */
  const uint volume_stack_size = kernel_data.volume_stack_size;

#ifdef __VOLUME_RECORD_ALL__
  Intersection hits[2 * MAX_VOLUME_STACK_SIZE + 1];
  uint num_hits = scene_intersect_volume_all(
      kg, &volume_ray, hits, 2 * volume_stack_size, PATH_RAY_ALL_VISIBILITY);
  if (num_hits > 0) {
    Intersection *isect = hits;

    qsort(hits, num_hits, sizeof(Intersection), intersections_compare);

    for (uint hit = 0; hit < num_hits; ++hit, ++isect) {
      shader_setup_from_ray(kg, stack_sd, &volume_ray, isect);
      volume_stack_enter_exit(INTEGRATOR_STATE_PASS, stack_sd);
    }
  }
#else
  Intersection isect;
  int step = 0;
  while (step < 2 * volume_stack_size &&
         scene_intersect_volume(kg, &volume_ray, &isect, PATH_RAY_ALL_VISIBILITY)) {
    shader_setup_from_ray(kg, stack_sd, &volume_ray, &isect);
    volume_stack_enter_exit(INTEGRATOR_STATE_PASS, stack_sd);

    /* Move ray forward. */
    volume_ray.P = ray_offset(stack_sd->P, -stack_sd->Ng);
    if (volume_ray.t != FLT_MAX) {
      volume_ray.D = normalize_len(to_P - volume_ray.P, &volume_ray.t);
    }
    ++step;
  }
#endif
}

ccl_device void integrator_intersect_volume_stack(INTEGRATOR_STATE_ARGS)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_VOLUME_STACK);

  ShaderDataTinyStorage stack_sd_storage;
  ShaderData *stack_sd = AS_SHADER_DATA(&stack_sd_storage);

  Ray volume_ray ccl_optional_struct_init;
  integrator_state_read_ray(INTEGRATOR_STATE_PASS, &volume_ray);
  volume_ray.t = FLT_MAX;

  const uint visibility = (INTEGRATOR_STATE(path, flag) & PATH_RAY_ALL_VISIBILITY);
  int stack_index = 0, enclosed_index = 0;

  /* Write background shader. */
  if (kernel_data.background.volume_shader != SHADER_NONE) {
    const VolumeStack new_entry = {OBJECT_NONE, kernel_data.background.volume_shader};
    integrator_state_write_volume_stack(INTEGRATOR_STATE_PASS, stack_index, new_entry);
    stack_index++;
  }

  /* Store to avoid global fetches on every intersection step. */
  const uint volume_stack_size = kernel_data.volume_stack_size;

#ifdef __VOLUME_RECORD_ALL__
  Intersection hits[2 * MAX_VOLUME_STACK_SIZE + 1];
  uint num_hits = scene_intersect_volume_all(
      kg, &volume_ray, hits, 2 * volume_stack_size, visibility);
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
          VolumeStack entry = integrator_state_read_volume_stack(INTEGRATOR_STATE_PASS, i);
          if (entry.object == stack_sd->object) {
            need_add = false;
            break;
          }
        }
        if (need_add && stack_index < volume_stack_size - 1) {
          const VolumeStack new_entry = {stack_sd->object, stack_sd->shader};
          integrator_state_write_volume_stack(INTEGRATOR_STATE_PASS, stack_index, new_entry);
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
  /* CUDA does not support defintion of a variable size arrays, so use the maximum possible. */
  int enclosed_volumes[MAX_VOLUME_STACK_SIZE];
  int step = 0;

  while (stack_index < volume_stack_size - 1 && enclosed_index < volume_stack_size - 1 &&
         step < 2 * volume_stack_size) {
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
        VolumeStack entry = integrator_state_read_volume_stack(INTEGRATOR_STATE_PASS, i);
        if (entry.object == stack_sd->object) {
          need_add = false;
          break;
        }
      }
      if (need_add) {
        const VolumeStack new_entry = {stack_sd->object, stack_sd->shader};
        integrator_state_write_volume_stack(INTEGRATOR_STATE_PASS, stack_index, new_entry);
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
    volume_ray.P = ray_offset(stack_sd->P, -stack_sd->Ng);
    ++step;
  }
#endif

  /* Write terminator. */
  const VolumeStack new_entry = {OBJECT_NONE, SHADER_NONE};
  integrator_state_write_volume_stack(INTEGRATOR_STATE_PASS, stack_index, new_entry);

  INTEGRATOR_PATH_NEXT(DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK,
                       DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
}

CCL_NAMESPACE_END
