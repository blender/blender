/*
 * Copyright 2011-2015 Blender Foundation
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

/* This kernel takes care of the logic to process "material of type holdout",
 * indirect primitive emission, bsdf blurring, probabilistic path termination
 * and AO.
 *
 * This kernels determines the rays for which a shadow_blocked() function
 * associated with AO should be executed. Those rays for which a
 * shadow_blocked() function for AO must be executed are marked with flag
 * RAY_SHADOW_RAY_CAST_ao and enqueued into the queue
 * QUEUE_SHADOW_RAY_CAST_AO_RAYS
 *
 * Ray state of rays that are terminated in this kernel are changed to RAY_UPDATE_BUFFER
 *
 * Note on Queues:
 * This kernel fetches rays from the queue QUEUE_ACTIVE_AND_REGENERATED_RAYS
 * and processes only the rays of state RAY_ACTIVE.
 * There are different points in this kernel where a ray may terminate and
 * reach RAY_UPDATE_BUFFER state. These rays are enqueued into
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. These rays will still be present
 * in QUEUE_ACTIVE_AND_REGENERATED_RAYS queue, but since their ray-state has
 * been changed to RAY_UPDATE_BUFFER, there is no problem.
 *
 * State of queues when this kernel is called:
 * At entry,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and
 *     RAY_REGENERATED rays
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_TO_REGENERATE rays.
 *   - QUEUE_SHADOW_RAY_CAST_AO_RAYS will be empty.
 * At exit,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE,
 *     RAY_REGENERATED and RAY_UPDATE_BUFFER rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_TO_REGENERATE and RAY_UPDATE_BUFFER rays.
 *   - QUEUE_SHADOW_RAY_CAST_AO_RAYS will be filled with rays marked with
 *     flag RAY_SHADOW_RAY_CAST_AO
 */

ccl_device void kernel_holdout_emission_blurring_pathtermination_ao(
    KernelGlobals *kg, ccl_local_param BackgroundAOLocals *locals)
{
  if (ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
    locals->queue_atomics_bg = 0;
    locals->queue_atomics_ao = 0;
  }
  ccl_barrier(CCL_LOCAL_MEM_FENCE);

#ifdef __AO__
  char enqueue_flag = 0;
#endif
  int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
  ray_index = get_ray_index(kg,
                            ray_index,
                            QUEUE_ACTIVE_AND_REGENERATED_RAYS,
                            kernel_split_state.queue_data,
                            kernel_split_params.queue_size,
                            0);

#ifdef __COMPUTE_DEVICE_GPU__
  /* If we are executing on a GPU device, we exit all threads that are not
   * required.
   *
   * If we are executing on a CPU device, then we need to keep all threads
   * active since we have barrier() calls later in the kernel. CPU devices,
   * expect all threads to execute barrier statement.
   */
  if (ray_index == QUEUE_EMPTY_SLOT) {
    return;
  }
#endif /* __COMPUTE_DEVICE_GPU__ */

#ifndef __COMPUTE_DEVICE_GPU__
  if (ray_index != QUEUE_EMPTY_SLOT) {
#endif

    ccl_global PathState *state = 0x0;
    float3 throughput;

    ccl_global char *ray_state = kernel_split_state.ray_state;
    ShaderData *sd = kernel_split_sd(sd, ray_index);

    if (IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
      uint buffer_offset = kernel_split_state.buffer_offset[ray_index];
      ccl_global float *buffer = kernel_split_params.tile.buffer + buffer_offset;

      ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
      ShaderData *emission_sd = AS_SHADER_DATA(&kernel_split_state.sd_DL_shadow[ray_index]);
      PathRadiance *L = &kernel_split_state.path_radiance[ray_index];

      throughput = kernel_split_state.throughput[ray_index];
      state = &kernel_split_state.path_state[ray_index];

      if (!kernel_path_shader_apply(kg, sd, state, ray, throughput, emission_sd, L, buffer)) {
        kernel_split_path_end(kg, ray_index);
      }
    }

    if (IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
      /* Path termination. this is a strange place to put the termination, it's
       * mainly due to the mixed in MIS that we use. gives too many unneeded
       * shader evaluations, only need emission if we are going to terminate.
       */
      float probability = path_state_continuation_probability(kg, state, throughput);

      if (probability == 0.0f) {
        kernel_split_path_end(kg, ray_index);
      }
      else if (probability < 1.0f) {
        float terminate = path_state_rng_1D(kg, state, PRNG_TERMINATE);
        if (terminate >= probability) {
          kernel_split_path_end(kg, ray_index);
        }
        else {
          kernel_split_state.throughput[ray_index] = throughput / probability;
        }
      }

      if (IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
        PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
        kernel_update_denoising_features(kg, sd, state, L);
      }
    }

#ifdef __AO__
    if (IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
      /* ambient occlusion */
      if (kernel_data.integrator.use_ambient_occlusion) {
        enqueue_flag = 1;
      }
    }
#endif /* __AO__ */

#ifndef __COMPUTE_DEVICE_GPU__
  }
#endif

#ifdef __AO__
  /* Enqueue to-shadow-ray-cast rays. */
  enqueue_ray_index_local(ray_index,
                          QUEUE_SHADOW_RAY_CAST_AO_RAYS,
                          enqueue_flag,
                          kernel_split_params.queue_size,
                          &locals->queue_atomics_ao,
                          kernel_split_state.queue_data,
                          kernel_split_params.queue_index);
#endif
}

CCL_NAMESPACE_END
