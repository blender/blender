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

/* This kernel takes care of scene_intersect function.
 *
 * This kernel changes the ray_state of RAY_REGENERATED rays to RAY_ACTIVE.
 * This kernel processes rays of ray state RAY_ACTIVE
 * This kernel determines the rays that have hit the background and changes
 * their ray state to RAY_HIT_BACKGROUND.
 */
ccl_device void kernel_scene_intersect(KernelGlobals *kg)
{
  /* Fetch use_queues_flag */
  char local_use_queues_flag = *kernel_split_params.use_queues_flag;
  ccl_barrier(CCL_LOCAL_MEM_FENCE);

  int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
  if (local_use_queues_flag) {
    ray_index = get_ray_index(kg,
                              ray_index,
                              QUEUE_ACTIVE_AND_REGENERATED_RAYS,
                              kernel_split_state.queue_data,
                              kernel_split_params.queue_size,
                              0);

    if (ray_index == QUEUE_EMPTY_SLOT) {
      return;
    }
  }

  /* All regenerated rays become active here */
  if (IS_STATE(kernel_split_state.ray_state, ray_index, RAY_REGENERATED)) {
#ifdef __BRANCHED_PATH__
    if (kernel_split_state.branched_state[ray_index].waiting_on_shared_samples) {
      kernel_split_path_end(kg, ray_index);
    }
    else
#endif /* __BRANCHED_PATH__ */
    {
      ASSIGN_RAY_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE);
    }
  }

  if (!IS_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE)) {
    return;
  }

  ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
  Ray ray = kernel_split_state.ray[ray_index];
  PathRadiance *L = &kernel_split_state.path_radiance[ray_index];

  Intersection isect;
  bool hit = kernel_path_scene_intersect(kg, state, &ray, &isect, L);
  kernel_split_state.isect[ray_index] = isect;

  if (!hit) {
    /* Change the state of rays that hit the background;
     * These rays undergo special processing in the
     * background_bufferUpdate kernel.
     */
    ASSIGN_RAY_STATE(kernel_split_state.ray_state, ray_index, RAY_HIT_BACKGROUND);
  }
}

CCL_NAMESPACE_END
