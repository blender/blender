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

ccl_device void kernel_enqueue_inactive(KernelGlobals *kg,
                                        ccl_local_param unsigned int *local_queue_atomics)
{
#ifdef __BRANCHED_PATH__
  /* Enqueue RAY_INACTIVE rays into QUEUE_INACTIVE_RAYS queue. */
  if (ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
    *local_queue_atomics = 0;
  }
  ccl_barrier(CCL_LOCAL_MEM_FENCE);

  int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);

  char enqueue_flag = 0;
  if (IS_STATE(kernel_split_state.ray_state, ray_index, RAY_INACTIVE)) {
    enqueue_flag = 1;
  }

  enqueue_ray_index_local(ray_index,
                          QUEUE_INACTIVE_RAYS,
                          enqueue_flag,
                          kernel_split_params.queue_size,
                          local_queue_atomics,
                          kernel_split_state.queue_data,
                          kernel_split_params.queue_index);
#endif /* __BRANCHED_PATH__ */
}

CCL_NAMESPACE_END
