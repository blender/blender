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

/*This kernel takes care of setting up ray for the next iteration of
 * path-iteration and accumulating radiance corresponding to AO and
 * direct-lighting
 *
 * Ray state of rays that are terminated in this kernel are changed
 * to RAY_UPDATE_BUFFER.
 *
 * Note on queues:
 * This kernel fetches rays from the queue QUEUE_ACTIVE_AND_REGENERATED_RAYS
 * and processes only the rays of state RAY_ACTIVE.
 * There are different points in this kernel where a ray may terminate and
 * reach RAY_UPDATE_BUFF state. These rays are enqueued into
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. These rays will still be present
 * in QUEUE_ACTIVE_AND_REGENERATED_RAYS queue, but since their ray-state has
 * been changed to RAY_UPDATE_BUFF, there is no problem.
 *
 * State of queues when this kernel is called:
 * At entry,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE,
 *     RAY_REGENERATED, RAY_UPDATE_BUFFER rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_TO_REGENERATE and RAY_UPDATE_BUFFER rays.
 * At exit,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE,
 *     RAY_REGENERATED and more RAY_UPDATE_BUFFER rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_TO_REGENERATE and more RAY_UPDATE_BUFFER rays.
 */

#ifdef __BRANCHED_PATH__
ccl_device_inline void kernel_split_branched_indirect_light_init(KernelGlobals *kg, int ray_index)
{
  kernel_split_branched_path_indirect_loop_init(kg, ray_index);

  ADD_RAY_FLAG(kernel_split_state.ray_state, ray_index, RAY_BRANCHED_LIGHT_INDIRECT);
}

ccl_device void kernel_split_branched_transparent_bounce(KernelGlobals *kg, int ray_index)
{
  ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
  ShaderData *sd = kernel_split_sd(sd, ray_index);
  ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
  ccl_global Ray *ray = &kernel_split_state.ray[ray_index];

#  ifdef __VOLUME__
  if (!(sd->flag & SD_HAS_ONLY_VOLUME)) {
#  endif
    /* continue in case of transparency */
    *throughput *= shader_bsdf_transparency(kg, sd);

    if (is_zero(*throughput)) {
      kernel_split_path_end(kg, ray_index);
      return;
    }

    /* Update Path State */
    path_state_next(kg, state, LABEL_TRANSPARENT);
#  ifdef __VOLUME__
  }
  else {
    if (!path_state_volume_next(kg, state)) {
      kernel_split_path_end(kg, ray_index);
      return;
    }
  }
#  endif

  ray->P = ray_offset(sd->P, -sd->Ng);
  ray->t -= sd->ray_length; /* clipping works through transparent */

#  ifdef __RAY_DIFFERENTIALS__
  ray->dP = sd->dP;
  ray->dD.dx = -sd->dI.dx;
  ray->dD.dy = -sd->dI.dy;
#  endif /* __RAY_DIFFERENTIALS__ */

#  ifdef __VOLUME__
  /* enter/exit volume */
  kernel_volume_stack_enter_exit(kg, sd, state->volume_stack);
#  endif /* __VOLUME__ */
}
#endif /* __BRANCHED_PATH__ */

ccl_device void kernel_next_iteration_setup(KernelGlobals *kg,
                                            ccl_local_param unsigned int *local_queue_atomics)
{
  if (ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
    *local_queue_atomics = 0;
  }
  ccl_barrier(CCL_LOCAL_MEM_FENCE);

  if (ccl_global_id(0) == 0 && ccl_global_id(1) == 0) {
    /* If we are here, then it means that scene-intersect kernel
     * has already been executed at least once. From the next time,
     * scene-intersect kernel may operate on queues to fetch ray index
     */
    *kernel_split_params.use_queues_flag = 1;

    /* Mark queue indices of QUEUE_SHADOW_RAY_CAST_AO_RAYS and
     * QUEUE_SHADOW_RAY_CAST_DL_RAYS queues that were made empty during the
     * previous kernel.
     */
    kernel_split_params.queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS] = 0;
    kernel_split_params.queue_index[QUEUE_SHADOW_RAY_CAST_DL_RAYS] = 0;
  }

  int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
  ray_index = get_ray_index(kg,
                            ray_index,
                            QUEUE_ACTIVE_AND_REGENERATED_RAYS,
                            kernel_split_state.queue_data,
                            kernel_split_params.queue_size,
                            0);

  ccl_global char *ray_state = kernel_split_state.ray_state;

#ifdef __VOLUME__
  /* Reactivate only volume rays here, most surface work was skipped. */
  if (IS_STATE(ray_state, ray_index, RAY_HAS_ONLY_VOLUME)) {
    ASSIGN_RAY_STATE(ray_state, ray_index, RAY_ACTIVE);
  }
#endif

  bool active = IS_STATE(ray_state, ray_index, RAY_ACTIVE);
  if (active) {
    ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
    ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
    ShaderData *sd = kernel_split_sd(sd, ray_index);
    ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
    PathRadiance *L = &kernel_split_state.path_radiance[ray_index];

#ifdef __BRANCHED_PATH__
    if (!kernel_data.integrator.branched || IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT)) {
#endif
      /* Compute direct lighting and next bounce. */
      if (!kernel_path_surface_bounce(kg, sd, throughput, state, &L->state, ray)) {
        kernel_split_path_end(kg, ray_index);
      }
#ifdef __BRANCHED_PATH__
    }
    else if (sd->flag & SD_HAS_ONLY_VOLUME) {
      kernel_split_branched_transparent_bounce(kg, ray_index);
    }
    else {
      kernel_split_branched_indirect_light_init(kg, ray_index);

      if (kernel_split_branched_path_surface_indirect_light_iter(
              kg, ray_index, 1.0f, kernel_split_sd(branched_state_sd, ray_index), true, true)) {
        ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
      }
      else {
        kernel_split_branched_path_indirect_loop_end(kg, ray_index);
        kernel_split_branched_transparent_bounce(kg, ray_index);
      }
    }
#endif /* __BRANCHED_PATH__ */
  }

  /* Enqueue RAY_UPDATE_BUFFER rays. */
  enqueue_ray_index_local(ray_index,
                          QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
                          IS_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER) && active,
                          kernel_split_params.queue_size,
                          local_queue_atomics,
                          kernel_split_state.queue_data,
                          kernel_split_params.queue_index);

#ifdef __BRANCHED_PATH__
  /* iter loop */
  if (ccl_global_id(0) == 0 && ccl_global_id(1) == 0) {
    kernel_split_params.queue_index[QUEUE_LIGHT_INDIRECT_ITER] = 0;
  }

  ray_index = get_ray_index(kg,
                            ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0),
                            QUEUE_LIGHT_INDIRECT_ITER,
                            kernel_split_state.queue_data,
                            kernel_split_params.queue_size,
                            1);

  if (IS_STATE(ray_state, ray_index, RAY_LIGHT_INDIRECT_NEXT_ITER)) {
    /* for render passes, sum and reset indirect light pass variables
     * for the next samples */
    PathRadiance *L = &kernel_split_state.path_radiance[ray_index];

    path_radiance_sum_indirect(L);
    path_radiance_reset_indirect(L);

    if (kernel_split_branched_path_surface_indirect_light_iter(
            kg, ray_index, 1.0f, kernel_split_sd(branched_state_sd, ray_index), true, true)) {
      ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
    }
    else {
      kernel_split_branched_path_indirect_loop_end(kg, ray_index);
      kernel_split_branched_transparent_bounce(kg, ray_index);
    }
  }

#  ifdef __VOLUME__
  /* Enqueue RAY_VOLUME_INDIRECT_NEXT_ITER rays */
  ccl_barrier(CCL_LOCAL_MEM_FENCE);
  if (ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
    *local_queue_atomics = 0;
  }
  ccl_barrier(CCL_LOCAL_MEM_FENCE);

  ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
  enqueue_ray_index_local(
      ray_index,
      QUEUE_VOLUME_INDIRECT_ITER,
      IS_STATE(kernel_split_state.ray_state, ray_index, RAY_VOLUME_INDIRECT_NEXT_ITER),
      kernel_split_params.queue_size,
      local_queue_atomics,
      kernel_split_state.queue_data,
      kernel_split_params.queue_index);

#  endif /* __VOLUME__ */

#  ifdef __SUBSURFACE__
  /* Enqueue RAY_SUBSURFACE_INDIRECT_NEXT_ITER rays */
  ccl_barrier(CCL_LOCAL_MEM_FENCE);
  if (ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
    *local_queue_atomics = 0;
  }
  ccl_barrier(CCL_LOCAL_MEM_FENCE);

  ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
  enqueue_ray_index_local(
      ray_index,
      QUEUE_SUBSURFACE_INDIRECT_ITER,
      IS_STATE(kernel_split_state.ray_state, ray_index, RAY_SUBSURFACE_INDIRECT_NEXT_ITER),
      kernel_split_params.queue_size,
      local_queue_atomics,
      kernel_split_state.queue_data,
      kernel_split_params.queue_index);
#  endif /* __SUBSURFACE__ */
#endif   /* __BRANCHED_PATH__ */
}

CCL_NAMESPACE_END
