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

/* This kernel takes care of rays that hit the background (sceneintersect
 * kernel), and for the rays of state RAY_UPDATE_BUFFER it updates the ray's
 * accumulated radiance in the output buffer. This kernel also takes care of
 * rays that have been determined to-be-regenerated.
 *
 * We will empty QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue in this kernel.
 *
 * Typically all rays that are in state RAY_HIT_BACKGROUND, RAY_UPDATE_BUFFER
 * will be eventually set to RAY_TO_REGENERATE state in this kernel.
 * Finally all rays of ray_state RAY_TO_REGENERATE will be regenerated and put
 * in queue QUEUE_ACTIVE_AND_REGENERATED_RAYS.
 *
 * State of queues when this kernel is called:
 * At entry,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_UPDATE_BUFFER, RAY_HIT_BACKGROUND, RAY_TO_REGENERATE rays.
 * At exit,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and
 *     RAY_REGENERATED rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be empty.
 */
ccl_device void kernel_buffer_update(KernelGlobals *kg,
                                     ccl_local_param unsigned int *local_queue_atomics)
{
  if (ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
    *local_queue_atomics = 0;
  }
  ccl_barrier(CCL_LOCAL_MEM_FENCE);

  int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
  if (ray_index == 0) {
    /* We will empty this queue in this kernel. */
    kernel_split_params.queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
  }
  char enqueue_flag = 0;
  ray_index = get_ray_index(kg,
                            ray_index,
                            QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
                            kernel_split_state.queue_data,
                            kernel_split_params.queue_size,
                            1);

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
#endif

#ifndef __COMPUTE_DEVICE_GPU__
  if (ray_index != QUEUE_EMPTY_SLOT) {
#endif

    ccl_global char *ray_state = kernel_split_state.ray_state;
    ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
    PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
    ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
    ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
    bool ray_was_updated = false;

    if (IS_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER)) {
      ray_was_updated = true;
      uint sample = state->sample;
      uint buffer_offset = kernel_split_state.buffer_offset[ray_index];
      ccl_global float *buffer = kernel_split_params.tile.buffer + buffer_offset;

      /* accumulate result in output buffer */
      kernel_write_result(kg, buffer, sample, L);

      ASSIGN_RAY_STATE(ray_state, ray_index, RAY_TO_REGENERATE);
    }

    if (kernel_data.film.cryptomatte_passes) {
      /* Make sure no thread is writing to the buffers. */
      ccl_barrier(CCL_LOCAL_MEM_FENCE);
      if (ray_was_updated && state->sample - 1 == kernel_data.integrator.aa_samples) {
        uint buffer_offset = kernel_split_state.buffer_offset[ray_index];
        ccl_global float *buffer = kernel_split_params.tile.buffer + buffer_offset;
        ccl_global float *cryptomatte_buffer = buffer + kernel_data.film.pass_cryptomatte;
        kernel_sort_id_slots(cryptomatte_buffer, 2 * kernel_data.film.cryptomatte_depth);
      }
    }

    if (IS_STATE(ray_state, ray_index, RAY_TO_REGENERATE)) {
      /* We have completed current work; So get next work */
      ccl_global uint *work_pools = kernel_split_params.work_pools;
      uint total_work_size = kernel_split_params.total_work_size;
      uint work_index;

      if (!get_next_work(kg, work_pools, total_work_size, ray_index, &work_index)) {
        /* If work is invalid, this means no more work is available and the thread may exit */
        ASSIGN_RAY_STATE(ray_state, ray_index, RAY_INACTIVE);
      }

      if (IS_STATE(ray_state, ray_index, RAY_TO_REGENERATE)) {
        ccl_global WorkTile *tile = &kernel_split_params.tile;
        uint x, y, sample;
        get_work_pixel(tile, work_index, &x, &y, &sample);

        /* Store buffer offset for writing to passes. */
        uint buffer_offset = (tile->offset + x + y * tile->stride) * kernel_data.film.pass_stride;
        kernel_split_state.buffer_offset[ray_index] = buffer_offset;

        /* Initialize random numbers and ray. */
        uint rng_hash;
        kernel_path_trace_setup(kg, sample, x, y, &rng_hash, ray);

        if (ray->t != 0.0f) {
          /* Initialize throughput, path radiance, Ray, PathState;
           * These rays proceed with path-iteration.
           */
          *throughput = make_float3(1.0f, 1.0f, 1.0f);
          path_radiance_init(L, kernel_data.film.use_light_pass);
          path_state_init(kg,
                          AS_SHADER_DATA(&kernel_split_state.sd_DL_shadow[ray_index]),
                          state,
                          rng_hash,
                          sample,
                          ray);
#ifdef __SUBSURFACE__
          kernel_path_subsurface_init_indirect(&kernel_split_state.ss_rays[ray_index]);
#endif
          ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
          enqueue_flag = 1;
        }
        else {
          ASSIGN_RAY_STATE(ray_state, ray_index, RAY_TO_REGENERATE);
        }
      }
    }

#ifndef __COMPUTE_DEVICE_GPU__
  }
#endif

  /* Enqueue RAY_REGENERATED rays into QUEUE_ACTIVE_AND_REGENERATED_RAYS;
   * These rays will be made active during next SceneIntersectkernel.
   */
  enqueue_ray_index_local(ray_index,
                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
                          enqueue_flag,
                          kernel_split_params.queue_size,
                          local_queue_atomics,
                          kernel_split_state.queue_data,
                          kernel_split_params.queue_index);
}

CCL_NAMESPACE_END
