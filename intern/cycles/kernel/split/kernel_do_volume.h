/*
 * Copyright 2011-2017 Blender Foundation
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

CCL_NAMESPACE_BEGIN

#if defined(__BRANCHED_PATH__) && defined(__VOLUME__)

ccl_device_inline void kernel_split_branched_path_volume_indirect_light_init(KernelGlobals *kg,
                                                                             int ray_index)
{
  kernel_split_branched_path_indirect_loop_init(kg, ray_index);

  ADD_RAY_FLAG(kernel_split_state.ray_state, ray_index, RAY_BRANCHED_VOLUME_INDIRECT);
}

ccl_device_noinline bool kernel_split_branched_path_volume_indirect_light_iter(KernelGlobals *kg,
                                                                               int ray_index)
{
  SplitBranchedState *branched_state = &kernel_split_state.branched_state[ray_index];

  ShaderData *sd = kernel_split_sd(sd, ray_index);
  PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
  ShaderData *emission_sd = AS_SHADER_DATA(&kernel_split_state.sd_DL_shadow[ray_index]);

  /* GPU: no decoupled ray marching, scatter probalistically */
  int num_samples = kernel_data.integrator.volume_samples;
  float num_samples_inv = 1.0f / num_samples;

  Ray volume_ray = branched_state->ray;
  volume_ray.t = (!IS_STATE(&branched_state->ray_state, 0, RAY_HIT_BACKGROUND)) ?
                     branched_state->isect.t :
                     FLT_MAX;

  float step_size = volume_stack_step_size(kg, branched_state->path_state.volume_stack);

  for (int j = branched_state->next_sample; j < num_samples; j++) {
    ccl_global PathState *ps = &kernel_split_state.path_state[ray_index];
    *ps = branched_state->path_state;

    ccl_global Ray *pray = &kernel_split_state.ray[ray_index];
    *pray = branched_state->ray;

    ccl_global float3 *tp = &kernel_split_state.throughput[ray_index];
    *tp = branched_state->throughput * num_samples_inv;

    /* branch RNG state */
    path_state_branch(ps, j, num_samples);

    /* integrate along volume segment with distance sampling */
    VolumeIntegrateResult result = kernel_volume_integrate(
        kg, ps, sd, &volume_ray, L, tp, step_size);

#  ifdef __VOLUME_SCATTER__
    if (result == VOLUME_PATH_SCATTERED) {
      /* direct lighting */
      kernel_path_volume_connect_light(kg, sd, emission_sd, *tp, &branched_state->path_state, L);

      /* indirect light bounce */
      if (!kernel_path_volume_bounce(kg, sd, tp, ps, &L->state, pray)) {
        continue;
      }

      /* start the indirect path */
      branched_state->next_closure = 0;
      branched_state->next_sample = j + 1;

      /* Attempting to share too many samples is slow for volumes as it causes us to
       * loop here more and have many calls to kernel_volume_integrate which evaluates
       * shaders. The many expensive shader evaluations cause the work load to become
       * unbalanced and many threads to become idle in this kernel. Limiting the
       * number of shared samples here helps quite a lot.
       */
      if (branched_state->shared_sample_count < 2) {
        if (kernel_split_branched_indirect_start_shared(kg, ray_index)) {
          continue;
        }
      }

      return true;
    }
#  endif
  }

  branched_state->next_sample = num_samples;

  branched_state->waiting_on_shared_samples = (branched_state->shared_sample_count > 0);
  if (branched_state->waiting_on_shared_samples) {
    return true;
  }

  kernel_split_branched_path_indirect_loop_end(kg, ray_index);

  /* todo: avoid this calculation using decoupled ray marching */
  float3 throughput = kernel_split_state.throughput[ray_index];
  kernel_volume_shadow(
      kg, emission_sd, &kernel_split_state.path_state[ray_index], &volume_ray, &throughput);
  kernel_split_state.throughput[ray_index] = throughput;

  return false;
}

#endif /* __BRANCHED_PATH__ && __VOLUME__ */

ccl_device void kernel_do_volume(KernelGlobals *kg)
{
#ifdef __VOLUME__
  /* We will empty this queue in this kernel. */
  if (ccl_global_id(0) == 0 && ccl_global_id(1) == 0) {
    kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
#  ifdef __BRANCHED_PATH__
    kernel_split_params.queue_index[QUEUE_VOLUME_INDIRECT_ITER] = 0;
#  endif /* __BRANCHED_PATH__ */
  }

  int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);

  if (*kernel_split_params.use_queues_flag) {
    ray_index = get_ray_index(kg,
                              ray_index,
                              QUEUE_ACTIVE_AND_REGENERATED_RAYS,
                              kernel_split_state.queue_data,
                              kernel_split_params.queue_size,
                              1);
  }

  ccl_global char *ray_state = kernel_split_state.ray_state;

  PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
  ccl_global PathState *state = &kernel_split_state.path_state[ray_index];

  if (IS_STATE(ray_state, ray_index, RAY_ACTIVE) ||
      IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND)) {
    ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
    ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
    ccl_global Intersection *isect = &kernel_split_state.isect[ray_index];
    ShaderData *sd = kernel_split_sd(sd, ray_index);
    ShaderData *emission_sd = AS_SHADER_DATA(&kernel_split_state.sd_DL_shadow[ray_index]);

    bool hit = !IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND);

    /* Sanitize volume stack. */
    if (!hit) {
      kernel_volume_clean_stack(kg, state->volume_stack);
    }
    /* volume attenuation, emission, scatter */
    if (state->volume_stack[0].shader != SHADER_NONE) {
      Ray volume_ray = *ray;
      volume_ray.t = (hit) ? isect->t : FLT_MAX;

#  ifdef __BRANCHED_PATH__
      if (!kernel_data.integrator.branched ||
          IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT)) {
#  endif /* __BRANCHED_PATH__ */
        float step_size = volume_stack_step_size(kg, state->volume_stack);

        {
          /* integrate along volume segment with distance sampling */
          VolumeIntegrateResult result = kernel_volume_integrate(
              kg, state, sd, &volume_ray, L, throughput, step_size);

#  ifdef __VOLUME_SCATTER__
          if (result == VOLUME_PATH_SCATTERED) {
            /* direct lighting */
            kernel_path_volume_connect_light(kg, sd, emission_sd, *throughput, state, L);

            /* indirect light bounce */
            if (kernel_path_volume_bounce(kg, sd, throughput, state, &L->state, ray)) {
              ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
            }
            else {
              kernel_split_path_end(kg, ray_index);
            }
          }
#  endif /* __VOLUME_SCATTER__ */
        }

#  ifdef __BRANCHED_PATH__
      }
      else {
        kernel_split_branched_path_volume_indirect_light_init(kg, ray_index);

        if (kernel_split_branched_path_volume_indirect_light_iter(kg, ray_index)) {
          ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
        }
      }
#  endif /* __BRANCHED_PATH__ */
    }
  }

#  ifdef __BRANCHED_PATH__
  /* iter loop */
  ray_index = get_ray_index(kg,
                            ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0),
                            QUEUE_VOLUME_INDIRECT_ITER,
                            kernel_split_state.queue_data,
                            kernel_split_params.queue_size,
                            1);

  if (IS_STATE(ray_state, ray_index, RAY_VOLUME_INDIRECT_NEXT_ITER)) {
    /* for render passes, sum and reset indirect light pass variables
     * for the next samples */
    path_radiance_sum_indirect(&kernel_split_state.path_radiance[ray_index]);
    path_radiance_reset_indirect(&kernel_split_state.path_radiance[ray_index]);

    if (kernel_split_branched_path_volume_indirect_light_iter(kg, ray_index)) {
      ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
    }
  }
#  endif /* __BRANCHED_PATH__ */

#endif /* __VOLUME__ */
}

CCL_NAMESPACE_END
